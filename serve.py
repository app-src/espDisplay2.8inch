import os
import glob
import time
from http.server import HTTPServer, BaseHTTPRequestHandler

# Configuration
HOST = "0.0.0.0"
PORT = 8000
IMAGE_FOLDER = "images"

# Global state
jpeg_cache = []
current_index = 0

def load_images():
    global jpeg_cache, current_index
    if not os.path.exists(IMAGE_FOLDER):
        os.makedirs(IMAGE_FOLDER)
        print(f"Created {IMAGE_FOLDER}/. Add JPEG files or run convert_gif.py first.")
        return

    files = sorted(glob.glob(os.path.join(IMAGE_FOLDER, "*.jpg")))
    
    if not files:
        print(f"No .jpg files in {IMAGE_FOLDER}/. Run convert_gif.py first.")
        jpeg_cache = []
        return

    jpeg_cache = []
    for f in files:
        with open(f, "rb") as fh:
            jpeg_cache.append(fh.read())
    
    current_index = 0
    print(f"Loaded {len(jpeg_cache)} JPEG frames ({sum(len(j) for j in jpeg_cache)/1024:.0f} KB total)")

def get_next_jpeg():
    global current_index
    if not jpeg_cache:
        load_images()
        if not jpeg_cache:
            return None

    # Reload when wrapping around to pick up new files
    if current_index == 0 and len(jpeg_cache) > 0:
        old_count = len(jpeg_cache)
        load_images()
        if len(jpeg_cache) != old_count:
            print(f"Reloaded: {old_count} -> {len(jpeg_cache)} images")

    data = jpeg_cache[current_index]
    current_index = (current_index + 1) % len(jpeg_cache)
    return data

class MJPEGHandler(BaseHTTPRequestHandler):
    def do_GET(self):
        if self.path == '/stream':
            self.send_response(200)
            self.send_header('Content-Type', 'multipart/x-mixed-replace; boundary=frame')
            self.end_headers()
            print("Stream started")
            try:
                while True:
                    data = get_next_jpeg()
                    if data:
                        self.wfile.write(
                            b'--frame\r\n'
                            b'Content-Type: image/jpeg\r\n'
                            b'Content-Length: ' + str(len(data)).encode() + b'\r\n'
                            b'\r\n'
                        )
                        self.wfile.write(data)
                        self.wfile.write(b'\r\n')
                        self.wfile.flush()
                    else:
                        time.sleep(1)
            except (BrokenPipeError, ConnectionResetError):
                print("Client disconnected")
            except Exception as e:
                print(f"Stream error: {e}")
        elif self.path == '/':
            self.send_response(200)
            self.send_header('Content-Type', 'text/html')
            self.end_headers()
            self.wfile.write(f"<h1>MJPEG Server</h1><p>{len(jpeg_cache)} frames</p><a href='/stream'>Stream</a>".encode())
        else:
            self.send_response(404)
            self.end_headers()

    def log_message(self, format, *args):
        pass

if __name__ == "__main__":
    load_images()
    server = HTTPServer((HOST, PORT), MJPEGHandler)
    print(f"MJPEG Server on http://localhost:{PORT}/stream")
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    server.server_close()
    print("Server stopped.")
