
#include <WiFi.h>
#include <SPI.h>
#include <TJpg_Decoder.h>
#include "Display_ST7789.h"

// ==========================================
// CONFIGURATION
// ==========================================
// Replace with your network credentials
const char* ssid     = "Skynet";
const char* password = "nhiptapswd";

// Replace with your Python User Server IP (Run ipconfig/ifconfig on PC)
const char* server_ip = "192.168.1.6"; 
const int server_port = 8000;
const char* server_path = "/stream";

// ==========================================

WiFiClient client;

// Callback function for TJpg_Decoder
// Signature must match: bool(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap)
bool tft_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap) {
  // Stop further decoding as image is running off screen
  if (y >= LCD_HEIGHT) return 0;
  
  // Render the decoded block
  LCD_addWindow(x, y, x + w - 1, y + h - 1, bitmap);
  
  // Return 1 to decode next block
  return 1;
}

void setup() {
  Serial.begin(115200);
  
  // Initialize Display
  Serial.println("Initializing Display...");
  LCD_Init();
  Backlight_Init();
  Set_Backlight(100); // Max brightness
  
  // Clear screen to black initially
  uint16_t black = 0x0000;
  LCD_addWindow(0, 0, LCD_WIDTH-1, LCD_HEIGHT-1, &black);

  // Configure TJpg_Decoder
  TJpgDec.setJpgScale(1);
  TJpgDec.setSwapBytes(false);
  TJpgDec.setCallback(tft_output);

  // Initialize WiFi
  Serial.printf("Connecting to %s", ssid);
  WiFi.begin(ssid, password);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if(WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi Connected!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nWiFi Connection Failed! Check credentials.");
  }
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    WiFi.begin(ssid, password);
    delay(1000);
    return;
  }

  if (!client.connected()) {
    Serial.print("Connecting to stream... ");
    if (client.connect(server_ip, server_port)) {
      Serial.println("Connected!");
      
      // Request /stream 
      client.print(String("GET ") + server_path + " HTTP/1.1\r\n" +
                   "Host: " + server_ip + "\r\n" +
                   "Connection: close\r\n\r\n");
    } else {
      Serial.println("Connection failed.");
      delay(1000);
      return;
    }
  }

  // MJPEG Stream Parsing
  // Find "Content-Length: "
  String response = client.readStringUntil('\n');

  if (response.startsWith("Content-Length: ")) {
      int len = response.substring(16).toInt();

      // Skip the double newline after headers
      client.readStringUntil('\n'); // blank line

      // Read JPEG data into buffer
      uint8_t* jpgBuffer = (uint8_t*)malloc(len);
      if (jpgBuffer) {
          int bytesRead = 0;
          while (bytesRead < len) {
              if (client.available()) {
                  bytesRead += client.read(jpgBuffer + bytesRead, len - bytesRead);
              }
          }
          
          unsigned long tStart = millis();
          
          // Draw the JPEG
          TJpgDec.drawJpg(0, 0, jpgBuffer, len);
          
          unsigned long tEnd = millis();
          float fps = 1000.0 / (tEnd - tStart);
          Serial.printf("Frame: %d bytes, Render: %lu ms (%.1f FPS)\n", len, (tEnd - tStart), fps);

          // Draw FPS on screen
          char fpsText[16];
          snprintf(fpsText, sizeof(fpsText), "%.1f FPS", fps);
          LCD_DrawString(5, 5, fpsText, 0x07E0, 0x0000, 2); // Green text on black

          free(jpgBuffer);
      } else {
          Serial.println("Malloc failed!");
          client.flush();
      }
  } else if (response.length() == 0) {
      if (!client.connected()) {
          Serial.println("Stream disconnected.");
          client.stop();
      }
  }
}
