
# WiFi Slideshow Setup Instructions

This project streams images from your PC (Python Server) to your ESP32 Display over WiFi.

## 1. Prerequisites
- **Python 3** installed on your PC.
- **Pillow** library for Python.
  ```bash
  pip install pillow
  ```
- **Images**: Create a folder named `images` in the same directory as `server.py` and add your PNG files there (resolution 240x320 recommended).

## 2. Server Setup (PC Side)
1. Open terminal in this folder.
2. Run the server:
   ```bash
   python3 server.py
   ```
3. Note your PC's IP address (e.g., `192.168.1.100`).

## 3. ESP32 Setup
1. Open `main.ino` in Arduino IDE or PlatformIO.
2. Edit the **CONFIGURATION** section at the top:
   - `ssid`: Your WiFi Name.
   - `password`: Your WiFi Password.
   - `server_ip`: The IP address of your PC (from step 2).
3. Connect your ESP32 and Flash the code.

## 4. How it works
- The ESP32 connects to WiFi.
- It requests `/stream` from the server.
- The server loops through the PNGs in the `images` folder, converts them to raw RGB565 pixel data, and streams them continuously.
- The ESP32 reads this stream and pushes it directly to the display, achieving simple video playback.
