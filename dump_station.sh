#!/bin/bash
# Simple script to dump radio station stream to disk

DURATION=${1:-60}  # Default 1 minute
OUTPUT_FILE=${2:-"station_dump_$(date +%Y%m%d_%H%M%S).mp3"}

echo "Dumping radio station for $DURATION seconds..."
echo "Output file: $OUTPUT_FILE"

# Activate virtual environment if it exists
if [ -d ".venv" ]; then
    source .venv/bin/activate
fi

# Run the Python dumper
python3 dump_station.py "$DURATION" "$OUTPUT_FILE"

# Show file info
if [ -f "$OUTPUT_FILE" ]; then
    echo ""
    echo "File created: $OUTPUT_FILE"
    echo "File size: $(ls -lh "$OUTPUT_FILE" | awk '{print $5}')"
    
    # Optionally show file info with ffprobe if available
    if command -v ffprobe &> /dev/null; then
        echo ""
        echo "Audio info:"
        ffprobe -v quiet -show_format -show_streams "$OUTPUT_FILE" | grep -E "(duration|bit_rate|codec_name|channels|sample_rate)"
    fi
else
    echo "Error: File was not created"
    exit 1
fi 