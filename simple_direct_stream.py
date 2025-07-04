#!/usr/bin/env python3
"""
Simple Direct Streaming Server
Downloads from Icecast radio and streams directly via FFmpeg (like MP3 server)
"""

import subprocess
import threading
import time
from http.server import HTTPServer, BaseHTTPRequestHandler
from socketserver import ThreadingMixIn
import requests
import tempfile
import os

class ThreadedHTTPServer(ThreadingMixIn, HTTPServer):
    daemon_threads = True

class DirectStreamHandler(BaseHTTPRequestHandler):
    
    def do_GET(self):
        if self.path == '/stream':
            self.stream_radio()
        elif self.path == '/':
            self.serve_info_page()
        else:
            self.send_error(404, "Not Found")
    
    def stream_radio(self):
        """Stream radio directly via FFmpeg"""
        client_ip = self.client_address[0]
        print(f"ESP32 direct stream started for {client_ip}")
        
        try:
            # Set response headers for PCM streaming
            self.send_response(200)
            self.send_header('Content-Type', 'audio/pcm')
            self.send_header('Cache-Control', 'no-cache')
            self.send_header('Connection', 'keep-alive')
            self.end_headers()
            
            # Use the exact same FFmpeg approach as the working MP3 server
            # But instead of reading from file, read from the radio stream URL
            radio_url = "https://icecast.octosignals.com/benziger"
            
            ffmpeg_cmd = [
                'ffmpeg',
                '-user_agent', 'VLC/3.0.16',
                '-reconnect', '1',
                '-reconnect_at_eof', '1',
                '-reconnect_streamed', '1',
                '-reconnect_delay_max', '2',
                '-i', radio_url,
                '-f', 's16le',
                '-acodec', 'pcm_s16le',
                '-ar', '32000',
                '-ac', '1',
                '-loglevel', 'error',  # Suppress verbose output
                '-'
            ]
            
            # Start ffmpeg process
            process = subprocess.Popen(
                ffmpeg_cmd,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                bufsize=0
            )
            
            # Stream PCM data in chunks - ESP32 controls the rate
            chunk_size = 3200  # 100ms of audio at 32kHz mono 16-bit
            
            start_time = time.time()
            chunks_sent = 0
            
            while True:
                chunk = process.stdout.read(chunk_size)
                if not chunk:
                    print(f"Stream ended for {client_ip}, restarting...")
                    break
                
                try:
                    self.wfile.write(chunk)
                    self.wfile.flush()
                    chunks_sent += 1
                    
                    # Log progress every 10 seconds of audio
                    if chunks_sent % 100 == 0:  # Every 10 seconds (100 * 0.1s)
                        elapsed = time.time() - start_time
                        audio_seconds = chunks_sent / 10.0
                        rate = audio_seconds / elapsed if elapsed > 0 else 0
                        print(f"Streaming to {client_ip}: {audio_seconds:.1f}s audio sent, {elapsed:.1f}s elapsed (rate: {rate:.1f}x)")
                        
                except (BrokenPipeError, ConnectionResetError):
                    print(f"Client {client_ip} disconnected")
                    break
            
            # Clean up process
            process.terminate()
            process.wait()
            
        except Exception as e:
            print(f"Error streaming to {client_ip}: {e}")
        finally:
            print(f"Direct stream ended for {client_ip}")
    
    def serve_info_page(self):
        """Serve information page"""
        html = f"""
        <!DOCTYPE html>
        <html>
        <head>
            <title>Direct Radio Streaming Server</title>
            <style>
                body {{ font-family: Arial, sans-serif; margin: 40px; }}
                .container {{ max-width: 600px; }}
                .status {{ background: #e8f5e8; padding: 15px; border-radius: 5px; }}
                .info {{ background: #f0f8ff; padding: 15px; border-radius: 5px; margin-top: 20px; }}
            </style>
        </head>
        <body>
            <div class="container">
                <h1>🎵 Direct Radio Streaming Server</h1>
                <div class="status">
                    <h3>Server Status: Running</h3>
                    <p><strong>Radio Source:</strong> https://icecast.octosignals.com/benziger</p>
                    <p><strong>Stream URL:</strong> <a href="/stream">http://localhost:8080/stream</a></p>
                    <p><strong>Format:</strong> PCM 32kHz, 16-bit, mono</p>
                    <p><strong>Method:</strong> Direct FFmpeg streaming</p>
                </div>
                <div class="info">
                    <h3>ESP32 Configuration</h3>
                    <p>Configure your ESP32 to connect to: <code>http://your-server-ip:8080/stream</code></p>
                    <p>Audio format: 32kHz sample rate, 16-bit depth, mono channel</p>
                </div>
            </div>
        </body>
        </html>
        """
        
        self.send_response(200)
        self.send_header('Content-Type', 'text/html')
        self.end_headers()
        self.wfile.write(html.encode('utf-8'))
    
    def log_message(self, format, *args):
        """Custom log format"""
        timestamp = time.strftime('[%H:%M:%S]')
        client_ip = self.client_address[0]
        print(f"{timestamp} {client_ip} - {format % args}")

def check_dependencies():
    """Check if required dependencies are available"""
    try:
        subprocess.run(['ffmpeg', '-version'], 
                      stdout=subprocess.DEVNULL, 
                      stderr=subprocess.DEVNULL, 
                      check=True)
        return True
    except (subprocess.CalledProcessError, FileNotFoundError):
        return False

def test_connection():
    """Test connection to radio stream"""
    try:
        response = requests.get("https://icecast.octosignals.com/benziger", 
                              headers={'User-Agent': 'VLC/3.0.16'}, 
                              timeout=10, 
                              stream=True)
        response.raise_for_status()
        print("✓ Radio stream connection successful")
        return True
    except Exception as e:
        print(f"✗ Cannot connect to radio stream: {e}")
        return False

def main():
    print("Direct Radio Streaming Server for ESP32")
    print("=" * 40)
    print("Radio Source: https://icecast.octosignals.com/benziger")
    print("Server starting on port 8080")
    print("Format: PCM 32kHz, 16-bit, mono")
    print("Stream URL: http://localhost:8080/stream")
    print("Web Interface: http://localhost:8080/")
    print("Method: Direct FFmpeg streaming (like MP3 server)")
    print("=" * 40)
    
    # Check dependencies
    if not check_dependencies():
        print("Error: ffmpeg is required but not found")
        print("Install with: sudo apt install ffmpeg")
        exit(1)
    
    # Test connection
    if not test_connection():
        print("Error: Cannot connect to radio stream")
        exit(1)
    
    # Start server
    server = ThreadedHTTPServer(('0.0.0.0', 8080), DirectStreamHandler)
    
    print("Server running on http://0.0.0.0:8080")
    print("Press Ctrl+C to stop")
    
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\nServer stopping...")
        server.shutdown()

if __name__ == "__main__":
    main() 