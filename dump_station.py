#!/usr/bin/env python3
"""
Simple script to dump radio station stream to disk for analysis
"""

import requests
import time
import sys
import os

RADIO_STREAM_URL = "https://icecast.octosignals.com/benziger"

def dump_station(duration_seconds=60, output_file="station_dump.mp3"):
    """Dump radio station stream to disk for specified duration"""
    
    print(f"Dumping {RADIO_STREAM_URL} for {duration_seconds} seconds...")
    print(f"Output file: {output_file}")
    
    try:
        # Setup HTTP session
        session = requests.Session()
        session.headers.update({
            'User-Agent': 'RadioBenziger/1.0 (Stream Dumper)',
            'Accept': 'audio/mpeg, audio/*',
            'Connection': 'keep-alive'
        })
        
        # Start streaming
        with session.get(RADIO_STREAM_URL, stream=True, timeout=10) as response:
            response.raise_for_status()
            
            print(f"Connected to stream. Status: {response.status_code}")
            print(f"Content-Type: {response.headers.get('Content-Type', 'unknown')}")
            
            with open(output_file, 'wb') as f:
                start_time = time.time()
                total_bytes = 0
                
                for chunk in response.iter_content(chunk_size=8192):
                    if chunk:
                        f.write(chunk)
                        total_bytes += len(chunk)
                        
                        # Check if we've reached the duration
                        elapsed = time.time() - start_time
                        if elapsed >= duration_seconds:
                            break
                        
                        # Progress update every 10 seconds
                        if int(elapsed) % 10 == 0 and elapsed > 0:
                            rate_kbps = (total_bytes * 8) / (elapsed * 1000)
                            print(f"  {elapsed:.0f}s: {total_bytes:,} bytes ({rate_kbps:.1f} kbps)")
                
                final_elapsed = time.time() - start_time
                final_rate_kbps = (total_bytes * 8) / (final_elapsed * 1000)
                
                print(f"\nDump complete!")
                print(f"Duration: {final_elapsed:.1f} seconds")
                print(f"Total bytes: {total_bytes:,}")
                print(f"Average bitrate: {final_rate_kbps:.1f} kbps")
                print(f"File size: {os.path.getsize(output_file):,} bytes")
                
    except Exception as e:
        print(f"Error dumping stream: {e}")
        return False
    
    return True

if __name__ == "__main__":
    duration = 60  # Default 1 minute
    output_file = "station_dump.mp3"
    
    # Parse command line arguments
    if len(sys.argv) > 1:
        try:
            duration = int(sys.argv[1])
        except ValueError:
            print("Usage: python3 dump_station.py [duration_seconds] [output_file]")
            sys.exit(1)
    
    if len(sys.argv) > 2:
        output_file = sys.argv[2]
    
    success = dump_station(duration, output_file)
    sys.exit(0 if success else 1) 