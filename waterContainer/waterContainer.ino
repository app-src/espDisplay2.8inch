
#include <SPI.h>
#include <Wire.h>
#include "Display_ST7789.h"
#include "Gyro_QMI8658.h"
#include "I2C_Driver.h"

// ==========================================
// PARTICLE SIMULATION CONFIG
// ==========================================
#define NUM_PARTICLES    450
#define PARTICLE_RADIUS  3
#define GRAVITY_SCALE    0.8f
#define DAMPING          0.95f
#define BOUNCE_LOSS      0.6f
#define MAX_VELOCITY     8.0f

// Colors (RGB565)
#define COLOR_BG         0x0011   // Very dark blue
#define COLOR_WATER      0x05DF   // Bright cyan
#define COLOR_WATER_DARK 0x033F   // Darker cyan for depth
#define COLOR_HIGHLIGHT  0x07FF   // White-cyan highlight
#define COLOR_WALL       0x4228   // Dark gray border

// ==========================================
// PARTICLE DATA
// ==========================================
struct Particle {
  float x, y;
  float vx, vy;
};

Particle particles[NUM_PARTICLES];

// Framebuffer in PSRAM (240*320*2 = 153,600 bytes)
uint16_t* framebuffer = NULL;

// ==========================================
// HELPER FUNCTIONS
// ==========================================

// Convert RGB to RGB565 (big-endian for SPI)
inline uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
  uint16_t c = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
  // Swap bytes for SPI transfer (big-endian)
  return (c >> 8) | (c << 8);
}

void fb_clear(uint16_t color) {
  for (uint32_t i = 0; i < (uint32_t)LCD_WIDTH * LCD_HEIGHT; i++) {
    framebuffer[i] = color;
  }
}

void fb_fillRect(int x0, int y0, int w, int h, uint16_t color) {
  for (int y = y0; y < y0 + h; y++) {
    if (y < 0 || y >= LCD_HEIGHT) continue;
    for (int x = x0; x < x0 + w; x++) {
      if (x < 0 || x >= LCD_WIDTH) continue;
      framebuffer[y * LCD_WIDTH + x] = color;
    }
  }
}

void fb_drawCircle(int cx, int cy, int r, uint16_t color) {
  int r2 = r * r;
  for (int dy = -r; dy <= r; dy++) {
    int py = cy + dy;
    if (py < 0 || py >= LCD_HEIGHT) continue;
    for (int dx = -r; dx <= r; dx++) {
      int px = cx + dx;
      if (px < 0 || px >= LCD_WIDTH) continue;
      if (dx * dx + dy * dy <= r2) {
        framebuffer[py * LCD_WIDTH + px] = color;
      }
    }
  }
}

// Draw a particle with a subtle highlight for 3D effect
void fb_drawParticle(int cx, int cy, int r) {
  int r2 = r * r;
  int r_inner = (r * r) / 2;  // highlight radius squared
  
  for (int dy = -r; dy <= r; dy++) {
    int py = cy + dy;
    if (py < 0 || py >= LCD_HEIGHT) continue;
    for (int dx = -r; dx <= r; dx++) {
      int px = cx + dx;
      if (px < 0 || px >= LCD_WIDTH) continue;
      int dist2 = dx * dx + dy * dy;
      if (dist2 <= r2) {
        uint16_t color;
        // Highlight in upper-left for 3D look
        if ((dx - 1) * (dx - 1) + (dy - 1) * (dy - 1) < r_inner) {
          color = COLOR_HIGHLIGHT;
        } else if (dist2 < r2 * 3 / 4) {
          color = COLOR_WATER;
        } else {
          color = COLOR_WATER_DARK;
        }
        framebuffer[py * LCD_WIDTH + px] = color;
      }
    }
  }
}

void fb_push() {
  LCD_addWindow(0, 0, LCD_WIDTH - 1, LCD_HEIGHT - 1, framebuffer);
}

// ==========================================
// SIMULATION
// ==========================================

void initParticles() {
  // Scatter particles in the center area
  for (int i = 0; i < NUM_PARTICLES; i++) {
    particles[i].x = PARTICLE_RADIUS + random(LCD_WIDTH - 2 * PARTICLE_RADIUS);
    particles[i].y = PARTICLE_RADIUS + random(LCD_HEIGHT - 2 * PARTICLE_RADIUS);
    particles[i].vx = 0;
    particles[i].vy = 0;
  }
}

void updateParticles(float gx, float gy) {
  float minX = PARTICLE_RADIUS;
  float maxX = LCD_WIDTH - 1 - PARTICLE_RADIUS;
  float minY = PARTICLE_RADIUS;
  float maxY = LCD_HEIGHT - 1 - PARTICLE_RADIUS;

  for (int i = 0; i < NUM_PARTICLES; i++) {
    Particle &p = particles[i];

    // Apply gravity from IMU
    p.vx += gx * GRAVITY_SCALE;
    p.vy += gy * GRAVITY_SCALE;

    // Apply damping
    p.vx *= DAMPING;
    p.vy *= DAMPING;

    // Clamp velocity
    if (p.vx > MAX_VELOCITY) p.vx = MAX_VELOCITY;
    if (p.vx < -MAX_VELOCITY) p.vx = -MAX_VELOCITY;
    if (p.vy > MAX_VELOCITY) p.vy = MAX_VELOCITY;
    if (p.vy < -MAX_VELOCITY) p.vy = -MAX_VELOCITY;

    // Move
    p.x += p.vx;
    p.y += p.vy;

    // Bounce off walls
    if (p.x < minX) { p.x = minX; p.vx = -p.vx * BOUNCE_LOSS; }
    if (p.x > maxX) { p.x = maxX; p.vx = -p.vx * BOUNCE_LOSS; }
    if (p.y < minY) { p.y = minY; p.vy = -p.vy * BOUNCE_LOSS; }
    if (p.y > maxY) { p.y = maxY; p.vy = -p.vy * BOUNCE_LOSS; }
  }

  // Grid-based particle-particle collision
  // Grid cell size = particle diameter, only check neighbors
  #define GRID_CELL (PARTICLE_RADIUS * 2)
  #define GRID_W (LCD_WIDTH / GRID_CELL + 1)
  #define GRID_H (LCD_HEIGHT / GRID_CELL + 1)
  #define MAX_PER_CELL 8
  
  static int16_t grid[GRID_W * GRID_H][MAX_PER_CELL];
  static uint8_t gridCount[GRID_W * GRID_H];
  
  // Clear grid
  memset(gridCount, 0, sizeof(gridCount));
  
  // Insert particles into grid
  for (int i = 0; i < NUM_PARTICLES; i++) {
    int gx_cell = (int)(particles[i].x) / GRID_CELL;
    int gy_cell = (int)(particles[i].y) / GRID_CELL;
    if (gx_cell < 0) gx_cell = 0;
    if (gx_cell >= GRID_W) gx_cell = GRID_W - 1;
    if (gy_cell < 0) gy_cell = 0;
    if (gy_cell >= GRID_H) gy_cell = GRID_H - 1;
    int idx = gy_cell * GRID_W + gx_cell;
    if (gridCount[idx] < MAX_PER_CELL) {
      grid[idx][gridCount[idx]++] = i;
    }
  }
  
  // Check collisions in neighboring cells
  float minDist = PARTICLE_RADIUS * 2.0f;
  float minDist2 = minDist * minDist;
  
  for (int cy = 0; cy < GRID_H; cy++) {
    for (int cx = 0; cx < GRID_W; cx++) {
      int cellIdx = cy * GRID_W + cx;
      if (gridCount[cellIdx] == 0) continue;
      
      // Check this cell + right, bottom, bottom-right, bottom-left neighbors
      for (int ny = cy; ny <= cy + 1 && ny < GRID_H; ny++) {
        for (int nx = (ny == cy ? cx : cx - 1); nx <= cx + 1 && nx < GRID_W; nx++) {
          if (nx < 0) continue;
          int neighborIdx = ny * GRID_W + nx;
          if (gridCount[neighborIdx] == 0) continue;
          
          int iStart = (cellIdx == neighborIdx) ? 0 : 0;
          for (int ii = 0; ii < gridCount[cellIdx]; ii++) {
            int jStart = (cellIdx == neighborIdx) ? ii + 1 : 0;
            for (int jj = jStart; jj < gridCount[neighborIdx]; jj++) {
              int pi = grid[cellIdx][ii];
              int pj = grid[neighborIdx][jj];
              
              float dx = particles[pj].x - particles[pi].x;
              float dy = particles[pj].y - particles[pi].y;
              float dist2 = dx * dx + dy * dy;
              
              if (dist2 < minDist2 && dist2 > 0.01f) {
                float dist = sqrtf(dist2);
                float overlap = (minDist - dist) * 0.5f;
                float fnx = dx / dist;
                float fny = dy / dist;
                
                particles[pi].x -= fnx * overlap;
                particles[pi].y -= fny * overlap;
                particles[pj].x += fnx * overlap;
                particles[pj].y += fny * overlap;
                
                float dvx = particles[pj].vx - particles[pi].vx;
                float dvy = particles[pj].vy - particles[pi].vy;
                float dot = dvx * fnx + dvy * fny;
                if (dot > 0) {
                  particles[pi].vx += dot * fnx * 0.3f;
                  particles[pi].vy += dot * fny * 0.3f;
                  particles[pj].vx -= dot * fnx * 0.3f;
                  particles[pj].vy -= dot * fny * 0.3f;
                }
              }
            }
          }
        }
      }
    }
  }
}

void renderFrame() {
  // Clear to dark blue background
  fb_clear(COLOR_BG);
  
  // Draw container border (2px)
  fb_fillRect(0, 0, LCD_WIDTH, 2, COLOR_WALL);            // Top
  fb_fillRect(0, LCD_HEIGHT - 2, LCD_WIDTH, 2, COLOR_WALL); // Bottom
  fb_fillRect(0, 0, 2, LCD_HEIGHT, COLOR_WALL);            // Left
  fb_fillRect(LCD_WIDTH - 2, 0, 2, LCD_HEIGHT, COLOR_WALL); // Right
  
  // Draw particles
  for (int i = 0; i < NUM_PARTICLES; i++) {
    fb_drawParticle((int)particles[i].x, (int)particles[i].y, PARTICLE_RADIUS);
  }
  
  // Push to display
  fb_push();
}

// ==========================================
// SETUP & LOOP
// ==========================================

void setup() {
  Serial.begin(115200);
  Serial.println("Water Container Simulation");
  
  // Initialize Display
  LCD_Init();
  Backlight_Init();
  Set_Backlight(100);
  
  // Clear screen
  uint16_t black = 0x0000;
  LCD_addWindow(0, 0, LCD_WIDTH - 1, LCD_HEIGHT - 1, &black);
  
  // Initialize I2C and IMU
  I2C_Init();
  delay(100);
  QMI8658_Init();
  Serial.println("IMU initialized");
  
  // Allocate framebuffer (try PSRAM first, then regular RAM)
  framebuffer = (uint16_t*)ps_malloc(LCD_WIDTH * LCD_HEIGHT * sizeof(uint16_t));
  if (!framebuffer) {
    framebuffer = (uint16_t*)malloc(LCD_WIDTH * LCD_HEIGHT * sizeof(uint16_t));
  }
  
  if (!framebuffer) {
    Serial.println("FATAL: Cannot allocate framebuffer!");
    while (1) delay(1000);
  }
  Serial.printf("Framebuffer allocated: %d bytes\n", LCD_WIDTH * LCD_HEIGHT * 2);
  
  // Initialize particles
  randomSeed(analogRead(0));
  initParticles();
  
  Serial.println("Starting simulation...");
}

void loop() {
  unsigned long tStart = millis();
  
  // Read IMU
  getAccelerometer();
  
  // Map IMU axes to screen axes (swapped and flipped)
  float gx = Accel.y;
  float gy = -Accel.x;
  
  // Update physics
  updateParticles(gx, gy);
  
  // Render
  renderFrame();
  
  // FPS logging
  unsigned long tEnd = millis();
  unsigned long dt = tEnd - tStart;
  float fps = (dt > 0) ? 1000.0f / dt : 999;
  
  // Draw FPS on screen (after framebuffer push, directly via text overlay)
  char fpsText[16];
  snprintf(fpsText, sizeof(fpsText), "%.0f FPS", fps);
  LCD_DrawString(5, 5, fpsText, 0x07E0, COLOR_BG, 1);
  
  Serial.printf("Frame: %lu ms (%.1f FPS) | Gx=%.2f Gy=%.2f\n", dt, fps, gx, gy);
}
