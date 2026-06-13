# T-Dongle-FileStation

A portable file server and storage solution for ESP32-S3 T-Dongle board with SD card support and built-in display.

## Features

- 📁 **File Management** - Upload, download, and delete files via web interface
- 📱 **WiFi Access Point** - Connect directly to the device without external network
- 🖥️ **Built-in Display** - Shows device status, IP address, and storage information
- 💾 **SD Card Storage** - Uses SPI mode for reliable SD card communication
- 🌐 **Web Interface** - Clean, responsive UI for file operations
- 📊 **Real-time Updates** - Automatic refresh of file list and storage info

## Hardware Requirements

- ESP32-S3 T-Dongle board
- Micro SD Card (formatted as FAT32)
- SD Card module/breakout board (if not built into T-Dongle)

## Pin Connections

Connect your SD card module to the following GPIO pins:

| SD Card Pin | GPIO Pin |
| ----------- | -------- |
| CS          | 47       |
| MOSI        | 18       |
| MISO        | 16       |
| SCK         | 17       |
| VCC         | 3.3V     |
| GND         | GND      |

## Software Dependencies

Install the following libraries in your Arduino IDE:

- **LovyanGFX** - Display driver (configured for T-Dongle)
- **ESPAsyncWebServer** - Async web server
- **SD** - SD card library (built-in)
- **SPI** - SPI communication (built-in)
- **WiFi** - WiFi functionality (built-in)
- **FS** - File system (built-in)

### Library Installation

1. Open Arduino IDE
2. Go to **Tools → Manage Libraries**
3. Search and install:
   - `LovyanGFX` by lovyan03
   - `ESPAsyncWebServer` by me-no-dev

## Configuration

### WiFi Access Point

The device creates its own WiFi network with these default settings:

- **SSID**: `TDongle-FileStation`
- **Password**: `12345678`
- **IP Address**: `192.168.4.1`

### Modifying WiFi Settings

Edit these lines in the code:

```cpp
const char *ap_ssid = "TDongle-FileStation";     // Change SSID
const char *ap_password = "12345678";            // Change password
```

### SD Card Pin Configuration

If your SD card uses different pins, modify these definitions:

```cpp
#define SD_CS 47
#define SD_MOSI 18
#define SD_MISO 16
#define SD_SCK 17
```

## Usage

### Setup Instructions

1. Upload the code to your ESP32-S3 T-Dongle

2. Insert SD card into the module

3. Power the device via USB or external power

4. Wait for initialization - The display will show status

5. Connect to WiFi:

- Search for TDongle-FileStation on your device

- Enter password 12345678

6. Open browser and navigate to http://192.168.4.1

### Web Interface Features

#### Upload Files

1. Click "Choose File" button

2. Select file from your computer

3. Click "Upload" button

4. File appears in the file list

#### Download Files

- Click on any filename in the list to download

#### Delete Files

- Click the red "Delete" button next to any file

#### System Information

- Shows SD card used/total space

- Displays number of files

- Updates automatically every 5 seconds

#### Display Information

The built-in display shows:

- Device name and status

- WiFi SSID and IP address

- SD card storage usage

- Number of files stored

- Connection instructions

#### File System Details

- Format: FAT32 recommended for SD cards

- Maximum filename length: Limited by SD card library (typically 8.3 format or long filenames depending on card)

- File operations: Upload, download, delete, list

- Auto-refresh: File list updates every 10 seconds

### API Endpoints

The device provides these REST API endpoints:

| Endpoint               | Method | Description                    |
| ---------------------- | ------ | ------------------------------ |
| /                      | GET    | Web interface HTML             |
| /api/files             | GET    | Get file list and storage info |
| /upload                | POST   | Upload file                    |
| /download/{filename}   | GET    | Download file                  |
| /api/delete/{filename} | DELETE | Delete file                    |

#### API Response Examples

**GET /api/files**

```json
{
  "total": 7636,
  "used": 245,
  "count": 3,
  "files": [
    { "name": "test.txt", "size": 1024 },
    { "name": "image.jpg", "size": 2048 }
  ]
}
```

### Troubleshooting

#### SD Card Not Detected

- Check connections - Verify all pins are correctly connected

- Try different speeds - Code automatically tries 400kHz, 1MHz, and 4MHz

- Check pull-up resistors - Internal pull-ups are enabled in code

- Verify card format - Format as FAT32

- Test with different SD card - Some cards may have compatibility issues

#### Display Issues

- Ensure LovyanGFX library is properly configured for T-Dongle

- Check LGFX_Setup.h file for correct display configuration

- Verify power supply (display needs stable voltage)

#### WiFi Connection Problems

- Make sure device is within range (typical AP range ~20-30 meters)

- Forget the network on your device and reconnect

- Check if other devices can connect

- Verify no interference on channel 1

#### Upload/Download Errors

- Check SD card free space

- Ensure filenames don't contain special characters

- Verify file permissions (SD card not write-protected)

#### Performance Notes

- SPI Speed: SD card communication starts at 400kHz and can increase to 4MHz

- File Transfer: Typical upload speeds ~200-500 KB/s depending on SD card

- Concurrent Users: Web server can handle multiple connections, but file operations are sequential

- Auto-refresh: File list updates every 10 seconds to minimize SD card wear
