
#include <WiFi.h>
#include <SPI.h>
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

// ==========================================

WiFiClient client;

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
    // Try to reconnect if lost
    WiFi.begin(ssid, password);
    delay(1000);
    return;
  }

  if (!client.connected()) {
    Serial.print("Connecting to stream... ");
    if (client.connect(server_ip, server_port)) {
      client.setTimeout(2000); // Set timeout to 2 seconds
      Serial.println("Connected!");
      
      // Request /stream for continuous video
      client.print(String("GET /stream HTTP/1.1\r\n") +
                   "Host: " + server_ip + "\r\n" +
                   "Connection: close\r\n\r\n");

      // Wait for headers
      unsigned long timeout = millis();
      while (client.available() == 0) {
        if (millis() - timeout > 5000) {
          Serial.println("Timeout waiting for data!");
          client.stop();
          return;
        }
      }

      // Read Headers
      bool headerEnded = false;
      while(client.connected() || client.available()) {
        String line = client.readStringUntil('\n');
        if (line == "\r" || line == "") { // Header end
            headerEnded = true;
            break;
        }
      }
      
      if (!headerEnded) {
         Serial.println("Invalid response headers");
         client.stop();
         return;
      }
      Serial.println("Stream started!");

    } else {
      Serial.println("Connection failed.");
      delay(1000);
      return;
    }
  }

  // Read frames continuously from the open stream
  if (client.connected()) {
      
      uint32_t imageSize = LCD_WIDTH * LCD_HEIGHT * 2;
      unsigned long tStart = millis();
      
      LCD_DrawStream(&client, imageSize);
      
      unsigned long tEnd = millis();
      float fps = 1000.0 / (tEnd - tStart);
      Serial.printf("Frame: %lu ms (%.1f FPS)\n", (tEnd - tStart), fps);
  } else {
      Serial.println("Stream disconnected.");
      delay(1000); // Retry connection soon
  }
}
