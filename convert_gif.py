"""
GIF to JPEG Frame Converter for ESP32 Display
Reads all .gif files from 'sourceGIF/' folder,
extracts each frame, converts to 240x320 RGB JPEG,
and saves to 'images/' folder.
"""

import os
import sys
import shutil
from PIL import Image

# Configuration
SOURCE_FOLDER = "sourceGIF"
OUTPUT_FOLDER = "images"
WIDTH = 240
HEIGHT = 320
JPEG_QUALITY = 85

def clear_output_folder():
    """Remove existing images in the output folder."""
    if os.path.exists(OUTPUT_FOLDER):
        count = len([f for f in os.listdir(OUTPUT_FOLDER) if os.path.isfile(os.path.join(OUTPUT_FOLDER, f))])
        if count > 0:
            response = input(f"'{OUTPUT_FOLDER}/' contains {count} files. Clear them? (y/n): ").strip().lower()
            if response == 'y':
                for f in os.listdir(OUTPUT_FOLDER):
                    fp = os.path.join(OUTPUT_FOLDER, f)
                    if os.path.isfile(fp):
                        os.remove(fp)
                print(f"Cleared {count} files.")
            else:
                print("Keeping existing files. New frames will be added alongside them.")
    else:
        os.makedirs(OUTPUT_FOLDER)
        print(f"Created '{OUTPUT_FOLDER}/' folder.")

def fit_and_crop(img, target_w, target_h):
    """
    Resize image to fill target dimensions (maintaining aspect ratio),
    then center-crop to exact size. No deformation.
    
    If source is landscape and target is portrait, rotate 90° first.
    """
    src_w, src_h = img.size
    src_is_landscape = src_w > src_h
    target_is_portrait = target_h > target_w
    
    # Auto-rotate landscape source for portrait display
    if src_is_landscape and target_is_portrait:
        img = img.rotate(90, expand=True)
        src_w, src_h = img.size
    
    # Calculate scale factor to FILL the target (no empty space)
    scale = max(target_w / src_w, target_h / src_h)
    new_w = int(src_w * scale)
    new_h = int(src_h * scale)
    
    # Resize maintaining aspect ratio
    img = img.resize((new_w, new_h), Image.LANCZOS)
    
    # Center crop to exact target size
    left = (new_w - target_w) // 2
    top = (new_h - target_h) // 2
    img = img.crop((left, top, left + target_w, top + target_h))
    
    return img

def extract_gif_frames(gif_path):
    """Extract all frames from a GIF and save as RGB JPEG."""
    gif_name = os.path.splitext(os.path.basename(gif_path))[0]
    
    try:
        gif = Image.open(gif_path)
    except Exception as e:
        print(f"  ERROR opening {gif_path}: {e}")
        return 0

    frame_count = 0
    
    try:
        while True:
            # Convert to RGBA first to handle transparency properly
            frame = gif.convert("RGBA")
            
            # Create black background
            background = Image.new("RGB", frame.size, (0, 0, 0))
            background.paste(frame, mask=frame.split()[3])
            
            # Fit and crop (no stretching/deformation)
            resized = fit_and_crop(background, WIDTH, HEIGHT)
            
            # Save as JPEG
            output_name = f"{gif_name}-{frame_count:04d}.jpg"
            output_path = os.path.join(OUTPUT_FOLDER, output_name)
            resized.save(output_path, "JPEG", quality=JPEG_QUALITY)
            
            frame_count += 1
            gif.seek(gif.tell() + 1)
            
    except EOFError:
        pass  # End of frames
    except Exception as e:
        print(f"  ERROR at frame {frame_count}: {e}")

    return frame_count

def main():
    # Check source folder
    if not os.path.exists(SOURCE_FOLDER):
        os.makedirs(SOURCE_FOLDER)
        print(f"Created '{SOURCE_FOLDER}/' folder.")
        print(f"Please put your .gif files in '{SOURCE_FOLDER}/' and run again.")
        return

    # Find GIF files
    gif_files = sorted([f for f in os.listdir(SOURCE_FOLDER) 
                       if f.lower().endswith('.gif')])
    
    if not gif_files:
        print(f"No .gif files found in '{SOURCE_FOLDER}/'")
        print(f"Please add GIF files and run again.")
        return

    print(f"Found {len(gif_files)} GIF file(s) in '{SOURCE_FOLDER}/':")
    for f in gif_files:
        gif = Image.open(os.path.join(SOURCE_FOLDER, f))
        n_frames = getattr(gif, 'n_frames', 1)
        print(f"  {f}: {gif.size}, {n_frames} frames, mode={gif.mode}")

    # Clear/prepare output
    clear_output_folder()

    # Process each GIF
    total_frames = 0
    for gif_file in gif_files:
        gif_path = os.path.join(SOURCE_FOLDER, gif_file)
        print(f"\nProcessing: {gif_file}")
        
        count = extract_gif_frames(gif_path)
        total_frames += count
        print(f"  Extracted {count} frames")

    print(f"\nDone! Total: {total_frames} JPEG frames saved to '{OUTPUT_FOLDER}/'")
    print(f"  Resolution: {WIDTH}x{HEIGHT}")
    print(f"  Quality: {JPEG_QUALITY}")
    
    # Verify a sample
    sample_files = sorted([f for f in os.listdir(OUTPUT_FOLDER) if f.endswith('.jpg')])[:3]
    if sample_files:
        print("\nSample verification:")
        for f in sample_files:
            img = Image.open(os.path.join(OUTPUT_FOLDER, f))
            size_kb = os.path.getsize(os.path.join(OUTPUT_FOLDER, f)) / 1024
            print(f"  {f}: mode={img.mode}, size={img.size}, {size_kb:.1f} KB")

    print(f"\nNow run: python convert_and_serve.py")

if __name__ == "__main__":
    main()
