# GearPulse - ESP Powered PC Hardware Monitor
**Version 1.0.0** | **Author: Shakil Ahmed** | **License: MIT**

![Beige Blue Minimalist Aesthetic Photo Collage Beach Desktop Wallpaper](https://github.com/user-attachments/assets/749d033a-b0ef-4ddd-a162-b2a8903294ea)

## Overview
GearPulse is an ESP8266-based hardware monitoring solution that displays real-time PC system metrics (CPU, GPU, RAM, and network usage) on a 16x2 LCD display. The system receives JSON data via serial communication from a computer and presents the information in an easy-to-read format.

## Hardware Requirements

- ESP8266 development board (NodeMCU, Wemos D1 Mini, etc.)
- 16x2 I2C LCD display (with I2C backpack)
- TTP223 capacitive touch sensor
- Breadboard and jumper wires
- USB cable for power and data connection
## Pin Diagram

### LCD Display Connections

| LCD Pin | ESP8266 Pin | Notes |
|---------|-------------|-------|
| SCL     | D1 (GPIO 5) | I2C Clock |
| SDA     | D2 (GPIO 4) | I2C Data |
| VCC     | 5V/3.3V     | Power (Check your LCD's voltage requirement) |
| GND     | GND         | Ground |

### TTP223 Touch Sensor Connections

| TTP223 Pin | ESP8266 Pin | Notes |
|------------|-------------|-------|
| OUT        | D5 (GPIO 14)| Touch Signal |
| VCC        | 3.3V        | Power |
| GND        | GND         | Ground |

## Connection Diagram
![GearPulse diagram](https://github.com/user-attachments/assets/052c5227-acf6-439c-99cc-d37d3df02786)


## Software Requirements

### Libraries
- LiquidCrystal_I2C
- ArduinoJson
- Ticker

### PC Software
- You need to install the GearPulse software on your PC to retrieve real-time hardware information. The software will send data through the COM3 serial port. You can download the latest version of GearPulse from: https://github.com/shakilahmed0369/GearPulse-ESP-Powered-PC-Hardware-Monitor/releases

## Installation and Setup

1. **Hardware Assembly**
   - Connect all components according to the pin diagram
   - Ensure proper power connections (GND and VCC)

2. **Software Installation**
   - Install VS Code and PlatformIO IDE extension
   - Clone or download the project from GitHub
   - Open the project folder in VS Code with PlatformIO
   - Connect your ESP8266 to your computer via USB
   - Click the "Build" button (✓) to compile the code
   - Click the "Upload" button (→) to flash the code to your ESP8266

3. **PC-Side Setup**
   - Download GearPulse Desktop from the [releases page](https://github.com/shakilahmed0369/GearPulse-ESP-Powered-PC-Hardware-Monitor/releases)
   - Run the installer and follow the setup wizard
   - After installation, launch GearPulse
   - Connect your ESP8266 to any USB port
   - The application will:
     - Automatically detect your ESP8266
     - Use the correct COM port (typically COM3)
     - Begin sending system data at 115200 baud rate
   - If needed, you can select a different COM port from the application settings

   The GearPulse PC application sends data in the following JSON format:
     ```json
     {
       "cpu": {
         "load": 25.5,
         "temp": 65.0
       },
       "gpu": {
         "load": 30.2,
         "temp": 70.0
       },
       "ram": {
         "total": 16.0,
         "used": 8.5,
         "usagePercent": 53.1
       },
       "network": {
         "upload": 256000.0,
         "download": 1048576.0
       }
     }
     ```

## Features

### Display Modes
The device cycles through three display modes:

1. **CPU/GPU Mode**
   - Shows CPU temperature (°C) and load (%)
   - Shows GPU temperature (°C) and load (%)

2. **Memory Mode**
   - Displays RAM usage (used/total GB and percentage)
   - Shows a graphical progress bar of RAM usage

3. **Network Mode**
   - Displays network upload and download speeds
   - Uses arrow indicators for data direction

### Controls
- **Short press** on the touch sensor: Change display mode
- **Long press** (2+ seconds) on the touch sensor: Toggle power on/off

## Operation

1. Connect the ESP8266 to your PC via USB
2. Power will automatically turn on when connected
3. Start your PC monitoring software to begin sending data
4. The display will show "System Ready" when it's waiting for data
5. Once data is received, it will display system information
6. Use the touch sensor to cycle through display modes

## Troubleshooting

| Issue | Possible Cause | Solution |
|-------|----------------|----------|
| Display not turning on | Power issue | Check connections, ensure ESP8266 is receiving power |
| No data displayed | Serial communication issue | Verify baud rate (115200), check if monitoring software is running |
| Display shows garbage characters | I2C address incorrect | Adjust the I2C address (default is 0x27, might be 0x3F for some displays) |
| Touch sensor not responding | Wiring issue | Check touch sensor connections |
| Firmware upload fails with serial port error (e.g., PermissionError 13) | Missing or incorrect CH340 driver | Install the CH340 driver from [this link](https://sparks.gogo.co.nz/ch340.html) |

## Technical Details

### Serial Communication
- Baud Rate: 115200
- Format: JSON
- Update Frequency: As provided by PC software

### Power Requirements
- Operating Voltage: 3.3V (ESP8266)
- Can be powered via USB connection to PC

## Credits
- Developed by Shakil Ahmed (2025)
- Website: https://shakilahmeed.com
- GitHub: https://github.com/ShakilAhmed0369

## License
This project is licensed under the MIT License.
