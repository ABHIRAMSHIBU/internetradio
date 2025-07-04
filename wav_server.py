#!/usr/bin/env python3
"""
Simple WAV Streaming Server for ESP32
Serves raw PCM data from WAV files directly to ESP32
"""

import asyncio
import logging
import os
import subprocess
import sys
import wave
from typing import Optional

from fastapi import FastAPI, Request, Response
from fastapi.responses import StreamingResponse
import uvicorn

# Configure logging
logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)

app = FastAPI(title="WAV Streaming Server", version="1.0.0")

# Global variables
current_wav_file: Optional[str] = None
clients_count = 0

def get_wav_info(wav_file: str) -> dict:
    """Get information about a WAV file"""
    try:
        with wave.open(wav_file, 'rb') as wav:
            info = {
                'channels': wav.getnchannels(),
                'sample_rate': wav.getframerate(),
                'sample_width': wav.getsampwidth(),
                'frames': wav.getnframes(),
                'duration': wav.getnframes() / wav.getframerate()
            }
            return info
    except Exception as e:
        logger.error(f"Error reading WAV file {wav_file}: {e}")
        return {}

def convert_to_esp32_format(wav_file: str, output_file: str) -> bool:
    """Convert WAV to ESP32 format (32kHz, 16-bit, mono)"""
    try:
        # Use ffmpeg to convert to ESP32 format
        import subprocess
        
        cmd = [
            'ffmpeg', '-y',
            '-i', wav_file,
            '-f', 's16le',
            '-acodec', 'pcm_s16le',
            '-ar', '32000',
            '-ac', '1',
            '-af', 'pan=mono|c0=0.5*c0+0.5*c1',
            output_file
        ]
        
        result = subprocess.run(cmd, capture_output=True, text=True)
        if result.returncode == 0:
            logger.info(f"Converted {wav_file} to ESP32 format: {output_file}")
            return True
        else:
            logger.error(f"FFmpeg conversion failed: {result.stderr}")
            return False
            
    except Exception as e:
        logger.error(f"Error converting WAV file: {e}")
        return False

async def stream_pcm_data_direct(wav_file: str, chunk_size: int = 3200):
    """Stream PCM data directly from FFmpeg (like MP3 server)"""
    global clients_count
    
    try:
        logger.info(f"Starting direct PCM stream from: {wav_file}")
        
        # Use same FFmpeg command as MP3 server for consistency
        ffmpeg_cmd = [
            'ffmpeg',
            '-i', wav_file,
            '-f', 's16le',
            '-acodec', 'pcm_s16le',
            '-ar', '32000',
            '-ac', '1',
            '-af', 'pan=mono|c0=0.5*c0+0.5*c1',  # Proper stereo to mono mix
            '-loglevel', 'error',  # Suppress verbose output
            '-'  # Output to stdout
        ]
        
        # Start ffmpeg process
        process = subprocess.Popen(
            ffmpeg_cmd,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            bufsize=0
        )
        
        bytes_sent = 0
        
        try:
            while True:
                chunk = process.stdout.read(chunk_size)
                if not chunk:
                    # Loop back for continuous playback
                    process.terminate()
                    process.wait()
                    
                    # Restart FFmpeg process
                    process = subprocess.Popen(
                        ffmpeg_cmd,
                        stdout=subprocess.PIPE,
                        stderr=subprocess.PIPE,
                        bufsize=0
                    )
                    chunk = process.stdout.read(chunk_size)
                    if not chunk:
                        break
                
                yield chunk
                bytes_sent += len(chunk)
                
                # No artificial delay - let ESP32 control the rate
                
        except Exception as e:
            logger.error(f"Error in streaming loop: {e}")
        finally:
            # Clean up process
            if process.poll() is None:
                process.terminate()
                process.wait()
                
    except Exception as e:
        logger.error(f"Error streaming PCM data: {e}")
    finally:
        clients_count -= 1
        logger.info(f"Client disconnected. Active clients: {clients_count}")

@app.get("/")
async def root():
    """Root endpoint with server info"""
    global current_wav_file
    
    info = {
        "server": "WAV Streaming Server",
        "version": "1.0.0",
        "current_file": current_wav_file,
        "active_clients": clients_count
    }
    
    if current_wav_file and os.path.exists(current_wav_file):
        wav_info = get_wav_info(current_wav_file)
        info.update(wav_info)
    
    return info

@app.get("/stream")
async def stream_wav(request: Request):
    """Stream PCM audio to ESP32"""
    global clients_count, current_wav_file
    
    client_ip = request.client.host
    logger.info(f"ESP32 streaming request from {client_ip}")
    
    if not current_wav_file or not os.path.exists(current_wav_file):
        return Response(
            content="No WAV file loaded. Use /load/{filename} to load a file.",
            status_code=404,
            media_type="text/plain"
        )
    
    clients_count += 1
    logger.info(f"ESP32 client connected. Active clients: {clients_count}")
    
    # Use direct streaming (no pre-conversion) like MP3 server
    return StreamingResponse(
        stream_pcm_data_direct(current_wav_file),
        media_type="application/octet-stream",
        headers={
            "Cache-Control": "no-cache",
            "Connection": "keep-alive",
            "Access-Control-Allow-Origin": "*"
        }
    )

@app.get("/load/{filename}")
async def load_wav_file(filename: str):
    """Load a WAV file for streaming"""
    global current_wav_file
    
    # Security: only allow files in current directory
    if '/' in filename or '..' in filename:
        return Response(
            content="Invalid filename",
            status_code=400,
            media_type="text/plain"
        )
    
    if not os.path.exists(filename):
        return Response(
            content=f"File not found: {filename}",
            status_code=404,
            media_type="text/plain"
        )
    
    current_wav_file = filename
    wav_info = get_wav_info(filename)
    
    logger.info(f"Loaded WAV file: {filename}")
    logger.info(f"  Channels: {wav_info.get('channels', 'unknown')}")
    logger.info(f"  Sample Rate: {wav_info.get('sample_rate', 'unknown')} Hz")
    logger.info(f"  Duration: {wav_info.get('duration', 0):.1f} seconds")
    
    return {
        "message": f"Loaded {filename}",
        "file_info": wav_info
    }

@app.get("/status")
async def get_status():
    """Get server status"""
    global current_wav_file, clients_count
    
    status = {
        "server": "WAV Streaming Server",
        "current_file": current_wav_file,
        "active_clients": clients_count,
        "file_exists": current_wav_file and os.path.exists(current_wav_file)
    }
    
    if current_wav_file and os.path.exists(current_wav_file):
        status["file_info"] = get_wav_info(current_wav_file)
        
        # Check if PCM version exists
        pcm_file = current_wav_file.replace('.wav', '.pcm').replace('.mp3', '.pcm')
        status["pcm_ready"] = os.path.exists(pcm_file)
        if status["pcm_ready"]:
            status["pcm_size"] = os.path.getsize(pcm_file)
    
    return status

def main():
    """Main entry point"""
    import argparse
    
    parser = argparse.ArgumentParser(description="WAV Streaming Server for ESP32")
    parser.add_argument("--file", "-f", help="WAV file to load on startup")
    parser.add_argument("--port", "-p", type=int, default=8080, help="Port to run on")
    parser.add_argument("--host", default="0.0.0.0", help="Host to bind to")
    
    args = parser.parse_args()
    
    # Load initial file if specified
    if args.file:
        global current_wav_file
        if os.path.exists(args.file):
            current_wav_file = args.file
            logger.info(f"Initial file loaded: {args.file}")
        else:
            logger.error(f"Initial file not found: {args.file}")
            sys.exit(1)
    
    logger.info(f"Starting WAV Streaming Server on {args.host}:{args.port}")
    if current_wav_file:
        logger.info(f"Ready to stream: {current_wav_file}")
    else:
        logger.info("No file loaded. Use /load/{filename} to load a file.")
    
    uvicorn.run(app, host=args.host, port=args.port)

if __name__ == "__main__":
    main() 