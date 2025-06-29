# Radio Benziger ESP32 - Simplified Development Plan

## Current Status: Phase 1 Complete ✅

### **What's Working Perfectly**
- ✅ **WiFi Persistence**: Credentials saved permanently, auto-reconnects
- ✅ **Web Interface**: Full configuration portal at device IP
- ✅ **Build System**: Three working scripts (`build.sh`, `flash.sh`, `build_flash.sh`)
- ✅ **Status Monitoring**: IP address every 5s, system status every 30s
- ✅ **EEPROM Backup**: Dual storage system for maximum reliability
- ✅ **Auto-Recovery**: Handles network failures gracefully

### **Memory & Performance**
- **Flash Usage**: 978KB (74%) - 26% available for audio libraries
- **RAM Usage**: 48KB (14%) - 86% available for audio processing  
- **Build Time**: ~15 seconds
- **WiFi Connection**: 5-15 seconds typical

---

## Next Session: Phase 2 - Add Audio 🎵

### **Session Goal**: Get Radio Benziger stream playing through speaker

### **Step 1: Hardware Setup** (10 minutes)
```
Connect MAX98357A I2S DAC:
ESP32 GPIO 26 → BCLK
ESP32 GPIO 25 → LRCK  
ESP32 GPIO 22 → DIN
3.3V → VDD, GND → GND
Connect speaker to MAX98357A output
```

### **Step 2: Install Audio Library** (5 minutes)
```bash
# Add to Arduino libraries
ESP32-audioI2S by schreibfaul1
```

### **Step 3: Add Audio Code** (30 minutes)
Create `AudioStreamer.h/.cpp` with:
- I2S initialization for MAX98357A
- HTTP stream connection to `https://icecast.octosignals.com/benziger`
- MP3 decoding and playback
- Volume control

### **Step 4: Web Interface Integration** (15 minutes)
Add to existing web pages:
- Volume slider
- Play/Stop button  
- "Now Playing" status
- Stream connection indicator

### **Step 5: Test & Debug** (30 minutes)
- Verify audio output
- Test web controls
- Check memory usage
- Validate stream stability

---

## Audio Implementation Details

### **Stream Specifications** (Already Analyzed)
- **URL**: `https://icecast.octosignals.com/benziger`
- **Format**: MP3 stereo, 32kHz, 96kbps
- **Perfect for ESP32**: Optimal sample rate and bitrate

### **Code Structure** (Add to existing)
```cpp
// AudioStreamer.h
class AudioStreamer {
public:
    static bool begin();                    // Initialize I2S + audio
    static bool connectToStream();          // Connect to Radio Benziger
    static void setVolume(uint8_t level);   // 0-100 volume control
    static bool isPlaying();                // Check playback status
    static String getStatus();              // Get stream status
    static void update();                   // Handle in loop()
};
```

### **Memory Allocation**
- **Audio Buffers**: 32KB (for smooth streaming)
- **HTTP Buffer**: 8KB (for stream data)
- **Total Audio RAM**: ~40KB (well within 86% available)

### **Integration Points**
- **Main Loop**: Add `AudioStreamer::update()`
- **Web Server**: Add volume/control endpoints
- **Status Reports**: Include audio status
- **WiFi Events**: Pause/resume audio on connection changes

---

## Simple Testing Plan

### **Phase 2 Success Criteria**
1. **Audio Output**: Clear sound from speaker
2. **Stream Connection**: Plays Radio Benziger stream
3. **Web Control**: Volume slider works
4. **Stability**: Runs for 30+ minutes without issues
5. **Recovery**: Resumes after WiFi reconnection

### **Quick Tests**
```bash
# 1. Build and flash
./build_flash.sh /dev/ttyUSB0

# 2. Connect to device WiFi (if needed) or use existing connection
# 3. Open web browser to device IP
# 4. Test volume control
# 5. Verify audio output
# 6. Monitor serial output for status
```

---

## Troubleshooting Guide

### **Common Audio Issues**
- **No Sound**: Check MAX98357A wiring, verify I2S pins
- **Distorted Audio**: Reduce volume, check power supply
- **Stream Cuts Out**: Monitor WiFi signal, check buffer sizes
- **Web Controls Don't Work**: Verify JavaScript, check server responses

### **Debug Information**
- **Serial Monitor**: Shows stream status, memory usage, errors
- **Web Interface**: Real-time status on main page
- **LED Indicator**: Solid = connected, blinking = issues

---

## File Structure After Phase 2

```
radiobenziger/
├── radiobenziger/
│   ├── radiobenziger.ino          # Main sketch (updated)
│   ├── Config.h/.cpp              # ✅ Complete
│   ├── WiFiManager.h/.cpp         # ✅ Complete  
│   ├── I2CScanner.h/.cpp          # ✅ Complete
│   └── AudioStreamer.h/.cpp       # 🆕 New for Phase 2
├── docs/                          # ✅ Complete documentation
├── build.sh, flash.sh, etc.      # ✅ Complete build system
└── build/                         # Generated files
```

---

## Expected Results After Phase 2

### **User Experience**
1. **Power On**: Device connects to WiFi automatically
2. **Web Interface**: Navigate to device IP address
3. **Start Stream**: Click play button or adjust volume
4. **Audio Output**: Radio Benziger plays through speaker
5. **Control**: Volume slider adjusts audio level
6. **Status**: Web page shows "Now Playing" with stream info

### **Technical Performance**
- **Stream Latency**: <2 seconds from button press to audio
- **Audio Quality**: Clear MP3 playback at 32kHz
- **Memory Usage**: <80% flash, <50% RAM
- **Reliability**: Handles network interruptions gracefully
- **Power Consumption**: Moderate increase due to audio processing

---

## Beyond Phase 2 (Future Sessions)

### **Phase 3: Polish & Features**
- Multiple stream support
- Equalizer controls
- Stream metadata display
- Mobile-friendly interface improvements
- OTA firmware updates

### **Optional Enhancements**
- Physical volume knob (rotary encoder)
- OLED display for status
- Multiple favorite stations
- Sleep timer functionality

---

## Key Success Factors

### **Why Phase 1 Succeeded**
1. **Dual Storage**: NVS + EEPROM backup prevented config loss
2. **Auto-Reconnection**: 30-second retry made WiFi reliable
3. **Modular Design**: Clean separation made debugging easy
4. **Comprehensive Monitoring**: Status reports caught issues early
5. **Robust Build System**: Consistent compilation and flashing

### **Phase 2 Strategy**
1. **Incremental Development**: Add audio without breaking WiFi
2. **Reuse Existing Infrastructure**: Leverage working web server
3. **Conservative Memory Usage**: Leave headroom for stability
4. **Extensive Testing**: Verify each component before integration
5. **Graceful Degradation**: Audio failure shouldn't crash system

---

## Ready for Next Session

The project is in an excellent state to begin Phase 2. All the difficult WiFi persistence and configuration challenges have been solved. The next session can focus purely on audio implementation, building on the solid foundation already in place.

**Key advantages for Phase 2:**
- ✅ Stable WiFi connection management
- ✅ Working web interface for controls
- ✅ Reliable configuration storage
- ✅ Comprehensive status monitoring
- ✅ Clean modular architecture
- ✅ Proven build and deployment system

The audio implementation can proceed with confidence that the underlying system is rock-solid and well-documented. 