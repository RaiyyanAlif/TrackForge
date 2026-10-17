# TrackForge

**Wireless Voice-Controlled Autonomous Line-Following Robot with Camera-Based Data Capture**

TrackForge is a battery-powered robotics project combining autonomous line following, wireless voice control, and camera-based image capture for data collection.

## Features

- Autonomous black-line following on a white surface
- Wireless voice commands over Wi-Fi
- ESP8266-based communication bridge
- Arduino Mega-based sensor and motor control
- Camera-enabled image capture using a phone and IP Webcam
- Battery-powered mobile operation

## Hardware

- Arduino Mega Mini Pro (ATmega2560)
- ESP8266 NodeMCU
- TB6612FNG motor driver
- 8-channel analog IR sensor array
- DC motors and battery
- Phone camera running IP Webcam

## Software

- Arduino C++
- Python
- SpeechRecognition
- pyttsx3
- Requests
- ESP8266 Wi-Fi communication

## Project Structure

- captain/ - Python voice-control application
- uild/ - Arduino Mega firmware
- esp8266_server/ - ESP8266 firmware
- ackups/ - Firmware backups

## Getting Started

1. Install Python, Git, and Arduino IDE.
2. Install the Python dependencies required by the voice-control application.
3. Configure the ESP8266 and camera settings locally.
4. Upload the appropriate firmware to the Arduino Mega and ESP8266.
5. Power the robot using a suitable regulated power supply.
6. Run the Python voice-control application on your computer.
7. Test wireless commands and autonomous line following.

## Camera Functionality

The phone camera provides images through IP Webcam for capture and data collection. On-device AI vision is not currently claimed as a feature.

## Security

Do not publish Wi-Fi passwords, API keys, private configuration files, or other credentials.

## License

A license can be added when the project owner chooses the reuse terms.
