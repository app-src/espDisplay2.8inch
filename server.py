
import os
import io
import time
import glob
from http.server import HTTPServer, BaseHTTPRequestHandler
from PIL import Image

# Configuration
HOST = "0.0.0.0"
PORT = 8000
IMAGE_FOLDER = "images"
WIDTH = 240
HEIGHT = 320

# Global state
image_files = []
current_index = 0

def load_images():
    global image_files
    if not os.path.exists(IMAGE_FOLDER):
        os.makedirs(IMAGE_FOLDER)
        print(f"Created {IMAGE_FOLDER} directory. Please add PNG files there.")
    
    # helper for sorting
    files = glob.glob(os.path.join(IMAGE_FOLDER, "*.png"))
    image_files = sorted(files)
    print(f"Found {len(image_files)} images.")

def get_next_image_bytes():
    global current_index
    if not image_files:
        return None
    
    # Get current file
    filename = image_files[current_index]
    
    # Advance index
    current_index = (current_index + 1) % len(image_files)
    
    try:
        img = Image.open(filename)
        img = img.convert("RGB")
        img = img.resize((WIDTH, HEIGHT))
        
        # Convert to RGB565
        # This is a bit slow in pure Python but works for testing
        # Optimization: Use numpy if available, but staying dependency-light here
        
        pixels = img.load()
        data = bytearray(WIDTH * HEIGHT * 2)
        idx = 0
        
        for y in range(HEIGHT):
            for x in range(WIDTH):
                r, g, b = pixels[x, y]
                
                # RGB565
                r5 = (r >> 3) & 0x1F
                g6 = (g >> 2) & 0x3F
                b5 = (b >> 3) & 0x1F
                
                rgb565 = (r5 << 11) | (g6 << 5) | b5
                
                # Big Endian for SPI
                high = (rgb565 >> 8) & 0xFF
                low = rgb565 & 0xFF
                
                data[idx] = high
                data[idx+1] = low
                idx += 2
                
        return data
        
    except Exception as e:
        print(f"Error processing {filename}: {e}")
        return None

class SimpleImageHandler(BaseHTTPRequestHandler):
    def do_GET(self):
        if self.path == '/next':
            data = get_next_image_bytes()
            if data:
                self.send_response(200)
                self.send_header('Content-Type', 'application/octet-stream')
                self.send_header('Content-Length', str(len(data)))
                self.end_headers()
                self.wfile.write(data)
            else:
                self.send_response(404)
                self.end_headers()
                self.wfile.write(b"No images found or error")

        elif self.path == '/stream':
             self.send_response(200)
             self.send_header('Content-Type', 'application/octet-stream')
             self.end_headers()
             print("Starting stream...")
             try:
                 while True:
                     data = get_next_image_bytes()
                     if data:
                         self.wfile.write(data)
                         self.wfile.flush()
                         # Small delay to control framerate if needed, 
                         # but user wants "as quick as possible"
                         # time.sleep(0.01) 
                     else:
                         break
             except BrokenPipeError:
                 print("Client disconnected.")
             except Exception as e:
                 print(f"Stream error: {e}")

        elif self.path == '/count':
             self.send_response(200)
             self.send_header('Content-Type', 'text/plain')
             self.end_headers()
             self.wfile.write(str(len(image_files)).encode())
        elif self.path == '/':
            self.send_response(200)
            self.send_header('Content-Type', 'text/html')
            self.end_headers()
            self.wfile.write(b"<h1>ESP32 Image Server</h1><p>Available endpoints: <a href='/count'>/count</a>, <a href='/next'>/next</a>, <a href='/stream'>/stream</a></p>")
        else:
            self.send_response(404)
            self.end_headers()
            self.wfile.write(b"404 Not Found")

def run_server():
    load_images()
    server = HTTPServer((HOST, PORT), SimpleImageHandler)
    server = HTTPServer((HOST, PORT), SimpleImageHandler)
    print(f"Server started. Access at http://localhost:{PORT}")
    print(f"Listening on {HOST}:{PORT}")
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    server.server_close()
    print("Server stopped.")

if __name__ == "__main__":
    run_server()
