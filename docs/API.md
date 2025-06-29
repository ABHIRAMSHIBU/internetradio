# Radio Benziger API Documentation

This document describes the REST API endpoints for the Radio Benziger ESP32 streaming project.

## 🌐 Base URL

When connected to WiFi: `http://<device_ip>/api/`
When in hotspot mode: `http://192.168.4.1/api/`

## 🎵 Audio Control Endpoints

### **Play Stream**
```http
POST /api/audio/play
Content-Type: application/json

{
    "url": "https://icecast.octosignals.com/benziger"
}
```

**Response:**
```json
{
    "status": "success",
    "message": "Stream started",
    "url": "https://icecast.octosignals.com/benziger"
}
```

### **Pause/Resume**
```http
POST /api/audio/pause
```

**Response:**
```json
{
    "status": "success",
    "message": "Audio paused",
    "state": "paused"
}
```

### **Stop Stream**
```http
POST /api/audio/stop
```

**Response:**
```json
{
    "status": "success",
    "message": "Audio stopped",
    "state": "stopped"
}
```

### **Volume Control**
```http
POST /api/audio/volume
Content-Type: application/json

{
    "volume": 75
}
```

**Response:**
```json
{
    "status": "success",
    "message": "Volume set",
    "volume": 75
}
```

### **Get Audio Status**
```http
GET /api/audio/status
```

**Response:**
```json
{
    "status": "success",
    "data": {
        "state": "playing",
        "volume": 75,
        "url": "https://icecast.octosignals.com/benziger",
        "buffer_level": 85,
        "bitrate": 128,
        "sample_rate": 44100
    }
}
```

## 📶 Network Management

### **Get WiFi Status**
```http
GET /api/network/status
```

**Response:**
```json
{
    "status": "success",
    "data": {
        "connected": true,
        "ssid": "MyWiFi",
        "ip": "192.168.1.100",
        "rssi": -45,
        "mode": "station"
    }
}
```

### **Scan WiFi Networks**
```http
GET /api/network/scan
```

**Response:**
```json
{
    "status": "success",
    "data": {
        "networks": [
            {
                "ssid": "MyWiFi",
                "rssi": -45,
                "encryption": "WPA2",
                "channel": 6
            },
            {
                "ssid": "NeighborWiFi",
                "rssi": -67,
                "encryption": "WPA2",
                "channel": 11
            }
        ]
    }
}
```

### **Connect to WiFi**
```http
POST /api/network/connect
Content-Type: application/json

{
    "ssid": "MyWiFi",
    "password": "mypassword"
}
```

**Response:**
```json
{
    "status": "success",
    "message": "Connected to WiFi",
    "ip": "192.168.1.100"
}
```

### **Start Hotspot Mode**
```http
POST /api/network/hotspot
Content-Type: application/json

{
    "ssid": "RadioBenziger-Setup",
    "password": "radiobenziger"
}
```

**Response:**
```json
{
    "status": "success",
    "message": "Hotspot started",
    "ip": "192.168.4.1"
}
```

## 🔧 Device Management

### **Get System Info**
```http
GET /api/system/info
```

**Response:**
```json
{
    "status": "success",
    "data": {
        "device_name": "Radio Benziger",
        "version": "1.0.0",
        "uptime": 3600,
        "free_heap": 234567,
        "cpu_freq": 240,
        "flash_size": 4194304,
        "chip_id": "ESP32-D0WD-V3"
    }
}
```

### **Scan I2C Devices**
```http
GET /api/devices/i2c/scan
```

**Response:**
```json
{
    "status": "success",
    "data": {
        "devices": [
            {
                "address": "0x3C",
                "name": "SSD1306 OLED",
                "responding": true
            },
            {
                "address": "0x68",
                "name": "DS3231 RTC",
                "responding": true
            }
        ]
    }
}
```

### **Get Device Status**
```http
GET /api/devices/status
```

**Response:**
```json
{
    "status": "success",
    "data": {
        "max98357a": {
            "present": true,
            "enabled": true,
            "sample_rate": 44100
        },
        "i2c_bus": {
            "active": true,
            "devices_count": 2
        },
        "gpio": {
            "buttons": {
                "play_pause": false,
                "volume_up": false,
                "volume_down": false
            },
            "leds": {
                "status": true,
                "wifi": true,
                "stream": false
            }
        }
    }
}
```

## ⚙️ Configuration

### **Get Configuration**
```http
GET /api/config
```

**Response:**
```json
{
    "status": "success",
    "data": {
        "device_name": "Radio Benziger",
        "stream_url": "https://icecast.octosignals.com/benziger",
        "volume": 75,
        "auto_start": true,
        "wifi": {
            "ssid": "MyWiFi",
            "saved": true
        }
    }
}
```

### **Update Configuration**
```http
PUT /api/config
Content-Type: application/json

{
    "device_name": "My Radio",
    "stream_url": "https://icecast.octosignals.com/benziger",
    "volume": 80,
    "auto_start": false
}
```

**Response:**
```json
{
    "status": "success",
    "message": "Configuration updated"
}
```

### **Factory Reset**
```http
POST /api/config/reset
```

**Response:**
```json
{
    "status": "success",
    "message": "Factory reset initiated"
}
```

## 🔄 System Control

### **Restart Device**
```http
POST /api/system/restart
```

**Response:**
```json
{
    "status": "success",
    "message": "Device restarting"
}
```

### **Get Logs**
```http
GET /api/system/logs
```

**Response:**
```json
{
    "status": "success",
    "data": {
        "logs": [
            {
                "timestamp": "2024-01-15T10:30:00Z",
                "level": "INFO",
                "message": "WiFi connected"
            },
            {
                "timestamp": "2024-01-15T10:30:05Z",
                "level": "INFO",
                "message": "Stream started"
            }
        ]
    }
}
```

## 📊 Statistics

### **Get Statistics**
```http
GET /api/stats
```

**Response:**
```json
{
    "status": "success",
    "data": {
        "uptime": 3600,
        "stream_time": 2400,
        "bytes_streamed": 12345678,
        "buffer_underruns": 2,
        "wifi_reconnects": 1,
        "memory_usage": {
            "heap_free": 234567,
            "heap_total": 327680,
            "heap_usage_percent": 28.4
        }
    }
}
```

## 🚨 Error Responses

### **Error Format**
```json
{
    "status": "error",
    "error": {
        "code": "INVALID_PARAMETER",
        "message": "Volume must be between 0 and 100"
    }
}
```

### **Common Error Codes**
- `INVALID_PARAMETER`: Invalid request parameter
- `NETWORK_ERROR`: WiFi or network connectivity issue
- `AUDIO_ERROR`: Audio system error
- `DEVICE_ERROR`: Hardware device error
- `CONFIG_ERROR`: Configuration error
- `INTERNAL_ERROR`: General system error

## 🔐 Authentication

Currently, the API is open and doesn't require authentication. In future versions, consider implementing:

- API key authentication
- Basic HTTP authentication
- JWT tokens for session management

## 📝 WebSocket Events

For real-time updates, the device also supports WebSocket connections:

```javascript
const ws = new WebSocket('ws://192.168.1.100/ws');

ws.onmessage = function(event) {
    const data = JSON.parse(event.data);
    console.log('Event:', data);
};
```

### **Event Types**
- `audio_state_changed`: Audio playback state changes
- `volume_changed`: Volume level changes
- `network_status_changed`: WiFi connection status
- `buffer_level_changed`: Audio buffer level updates
- `error_occurred`: System errors

This API provides comprehensive control over your Radio Benziger device! 