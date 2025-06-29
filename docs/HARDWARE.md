# Hardware Setup Guide

This document provides detailed hardware setup instructions for the Radio Benziger ESP32 project.

## 🔌 MAX98357A I2S DAC Wiring

### **Connection Diagram**
```
ESP32-WROOM-32          MAX98357A
┌─────────────┐        ┌─────────────┐
│             │        │             │
│ GPIO 26  ───┼────────┼─── BCLK     │
│ GPIO 25  ───┼────────┼─── LRCK/WS  │
│ GPIO 22  ───┼────────┼─── DIN      │
│ 3.3V     ───┼────────┼─── VDD      │
│ GND      ───┼────────┼─── GND      │
│ GPIO 21  ───┼────────┼─── SD       │ (optional)
│             │        │             │
└─────────────┘        └─────────────┘
                              │
                              │ Audio Out
                              ▼
                       ┌─────────────┐
                       │   Speaker   │
                       │   4-8 Ohm   │
                       │   3-5 Watt  │
                       └─────────────┘
```

### **Pin Descriptions**
- **BCLK (Bit Clock)**: Synchronizes data transmission
- **LRCK/WS (Word Select)**: Left/Right channel selection
- **DIN (Data Input)**: Audio data stream
- **VDD**: 3.3V power supply
- **GND**: Ground connection
- **SD (Shutdown)**: Optional enable/disable control

## 🔧 I2C Bus Setup

### **I2C Wiring**
```
ESP32 Pin    | I2C Function | Wire Color (suggested)
-------------|--------------|----------------------
GPIO 21      | SDA          | Blue
GPIO 22      | SCL          | Yellow
3.3V         | VCC          | Red
GND          | GND          | Black
```

### **Pull-up Resistors**
- **Required**: 4.7kΩ resistors from SDA and SCL to 3.3V
- **Location**: Can be on breadboard or PCB
- **Note**: Some modules have built-in pull-ups

### **I2C Device Addresses**
Common devices you might connect:
```
Device               | Address (7-bit) | Purpose
---------------------|-----------------|------------------
OLED Display (SSD1306)| 0x3C or 0x3D   | Status display
RTC Module (DS3231)   | 0x68           | Real-time clock
Temperature (BME280)  | 0x76 or 0x77   | Environmental sensor
EEPROM (24C32)       | 0x50-0x57      | Additional storage
```

## 🎛️ Control Interface

### **Button Wiring**
```
ESP32 Pin    | Function      | Wiring
-------------|---------------|------------------
GPIO 4       | Play/Pause    | Button to GND + 10kΩ pullup
GPIO 5       | Volume Up     | Button to GND + 10kΩ pullup
GPIO 18      | Volume Down   | Button to GND + 10kΩ pullup
GPIO 19      | Config Mode   | Button to GND + 10kΩ pullup
```

### **LED Indicators**
```
ESP32 Pin    | Function      | Wiring
-------------|---------------|------------------
GPIO 2       | Status LED    | Built-in LED (blue)
GPIO 23      | Power LED     | LED + 220Ω resistor to GND
GPIO 16      | WiFi LED      | LED + 220Ω resistor to GND
GPIO 17      | Stream LED    | LED + 220Ω resistor to GND
```

## 🔋 Power Considerations

### **Power Supply Requirements**
- **ESP32**: 3.3V, ~240mA (active with WiFi)
- **MAX98357A**: 3.3V, ~10mA (idle), ~1A (full output)
- **Speaker**: Depends on volume and impedance
- **Total**: Plan for 2A minimum at 5V input

### **Power Distribution**
```
USB 5V Input
    │
    ├── ESP32 Dev Board (has onboard 3.3V regulator)
    │       │
    │       └── 3.3V to MAX98357A
    │
    └── Optional: External 3.3V regulator for higher current
```

## 🧪 Testing Setup

### **Basic Audio Test Circuit**
1. Connect MAX98357A as shown above
2. Connect a small speaker (4-8Ω, 3-5W)
3. Upload test sketch with sine wave generator
4. Verify audio output

### **I2C Scanner Test**
1. Wire I2C bus with pull-up resistors
2. Connect any I2C device (OLED display recommended)
3. Upload I2C scanner sketch
4. Check serial monitor for detected devices

## 🛠️ Assembly Tips

### **Breadboard Layout**
```
Power Rails: 3.3V and GND distributed
Left Side:   ESP32 development board
Right Side:  MAX98357A module
Bottom:      I2C devices and pull-up resistors
Top:         Control buttons and LEDs
```

### **PCB Considerations**
- Keep audio traces short and away from digital signals
- Use ground plane for noise reduction
- Separate analog and digital grounds if possible
- Add bypass capacitors (0.1µF) near power pins

## 📏 Physical Dimensions

### **ESP32-WROOM-32 Dev Board**
- **Size**: ~55mm x 28mm
- **Height**: ~15mm (with components)
- **Mounting**: 4 holes, M3 screws

### **MAX98357A Module**
- **Size**: ~20mm x 15mm
- **Height**: ~5mm
- **Mounting**: 2.54mm pin spacing

## 🔊 Speaker Selection

### **Recommended Specifications**
- **Impedance**: 4Ω or 8Ω
- **Power**: 3-5 watts
- **Size**: 2-3 inches for desktop use
- **Type**: Full-range driver for internet radio

### **Speaker Wiring**
```
MAX98357A Output    Speaker
┌─────────────┐    ┌─────────────┐
│ OUT+     ───┼────┼─── +        │
│ OUT-     ───┼────┼─── -        │
└─────────────┘    └─────────────┘
```

## ⚠️ Safety Notes

### **Electrical Safety**
- Always disconnect power when wiring
- Check connections before applying power
- Use appropriate wire gauge (22-24 AWG for signals)
- Ensure proper polarity for powered components

### **Audio Safety**
- Start with low volume settings
- Protect your hearing during testing
- Use appropriate speaker ratings
- Avoid short circuits on audio outputs

## 🔍 Troubleshooting

### **No Audio Output**
1. Check I2S pin connections
2. Verify 3.3V power to MAX98357A
3. Confirm speaker wiring and impedance
4. Test with simple sine wave

### **I2C Devices Not Detected**
1. Verify SDA/SCL connections
2. Check pull-up resistors (4.7kΩ)
3. Confirm device addresses
4. Test with I2C scanner

### **WiFi Connection Issues**
1. Check antenna connection (built-in)
2. Verify power supply stability
3. Test in different locations
4. Check for interference sources

This hardware setup will provide a solid foundation for your Radio Benziger streaming project! 