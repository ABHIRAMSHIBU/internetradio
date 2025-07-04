#!/usr/bin/env python3
"""
MP3 to PCM Streaming Server for Radio Benziger ESP32
Converts MP3 file to mono 32kHz PCM and streams over HTTP
"""

import os
import sys
import threading
import time
from http.server import HTTPServer, BaseHTTPRequestHandler
from socketserver import ThreadingMixIn
import subprocess
import signal

class ThreadedHTTPServer(ThreadingMixIn, HTTPServer):
    daemon_threads = True

class MP3StreamHandler(BaseHTTPRequestHandler):
    def __init__(self, *args, mp3_file=None, **kwargs):
        self.mp3_file = mp3_file
        super().__init__(*args, **kwargs)
    
    def do_GET(self):
        if self.path == '/stream':
            self.stream_mp3()
        elif self.path == '/':
            self.serve_info_page()
        else:
            self.send_error(404, "Not Found")
    
    def stream_mp3(self):
        """Stream MP3 file as PCM audio"""
        client_ip = self.client_address[0]
        print(f"MP3 PCM stream started for {client_ip}")
        
        try:
            # Set response headers for PCM streaming
            self.send_response(200)
            self.send_header('Content-Type', 'audio/pcm')
            self.send_header('Cache-Control', 'no-cache')
            self.send_header('Connection', 'keep-alive')
            self.end_headers()
            
            # Use ffmpeg to convert MP3 to PCM with explicit left channel mono
            # -i: input file
            # -f s16le: 16-bit signed little-endian PCM
            # -acodec pcm_s16le: PCM 16-bit codec
            # -ar 32000: 32kHz sample rate
            # -ac 1: mono (1 channel)
            # -af pan=mono|c0=0.5*c0+0.5*c1: mix stereo to mono properly
            # -map_channel 0.0.0: use only left channel as fallback
            # -: output to stdout
            ffmpeg_cmd = [
                'ffmpeg',
                '-i', self.mp3_file,
                '-f', 's16le',
                '-acodec', 'pcm_s16le',
                '-ar', '32000',
                '-ac', '1',
                '-af', 'pan=mono|c0=0.5*c0+0.5*c1',  # Proper stereo to mono mix
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
            
            # Stream PCM data in chunks - ESP32 controls the rate via intelligent flow control
            chunk_size = 3200  # 100ms of audio at 32kHz mono 16-bit (32000 * 0.1 * 2)
            
            start_time = time.time()
            chunks_sent = 0
            
            while True:
                chunk = process.stdout.read(chunk_size)
                if not chunk:
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
            print(f"MP3 PCM stream ended for {client_ip}")
    
    def serve_info_page(self):
        """Serve information page"""
        html = f"""
        <!DOCTYPE html>
        <html>
        <head>
            <title>MP3 PCM Streaming Server</title>
            <style>
                body {{ font-family: Arial, sans-serif; margin: 40px; }}
                .container {{ max-width: 600px; }}
                .status {{ background: #e8f5e8; padding: 15px; border-radius: 5px; }}
                .info {{ background: #f0f8ff; padding: 15px; border-radius: 5px; margin-top: 20px; }}
            </style>
        </head>
        <body>
            <div class="container">
                <h1>🎵 MP3 PCM Streaming Server</h1>
                <div class="status">
                    <h3>Server Status: Running</h3>
                    <p><strong>MP3 File:</strong> {self.mp3_file}</p>
                    <p><strong>Stream URL:</strong> <a href="/stream">http://localhost:8080/stream</a></p>
                    <p><strong>Format:</strong> PCM 32kHz, 16-bit, mono</p>
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

def main():
    if len(sys.argv) != 2:
        print("Usage: python3 mp3_stream_server.py <mp3_file>")
        print("Example: python3 mp3_stream_server.py /home/user/music.mp3")
        sys.exit(1)
    
    mp3_file = sys.argv[1]
    
    # Check if MP3 file exists
    if not os.path.exists(mp3_file):
        print(f"Error: MP3 file '{mp3_file}' not found")
        sys.exit(1)
    
    # Check dependencies
    if not check_dependencies():
        print("Error: ffmpeg is required but not found")
        print("Install with: sudo apt install ffmpeg")
        sys.exit(1)
    
    print("MP3 PCM Streaming Server for Radio Benziger ESP32")
    print("=" * 50)
    print(f"MP3 File: {mp3_file}")
    print("Server starting on port 8080")
    print("Format: PCM 32kHz, 16-bit, mono")
    print("Stream URL: http://localhost:8080/stream")
    print("Web Interface: http://localhost:8080/")
    print("Mode: Pull-based streaming (ESP32 controls rate)")
    print("=" * 50)
    
    # Create handler with MP3 file
    def handler(*args, **kwargs):
        return MP3StreamHandler(*args, mp3_file=mp3_file, **kwargs)
    
    # Start server
    server = ThreadedHTTPServer(('0.0.0.0', 8080), handler)
    
    def signal_handler(sig, frame):
        print("\nServer stopping...")
        server.shutdown()
        print("Server stopped.")
        sys.exit(0)
    
    signal.signal(signal.SIGINT, signal_handler)
    
    print(f"Server running on http://0.0.0.0:8080")
    print("Press Ctrl+C to stop")
    
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.shutdown()

if __name__ == "__main__":
    main() 