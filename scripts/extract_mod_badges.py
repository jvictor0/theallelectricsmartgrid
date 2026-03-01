#!/usr/bin/env python3
"""
Convert black background to transparent in modulator badge sprite sheet.
No cropping - just transparency conversion.
"""

from PIL import Image
import os


def convert_to_white_on_transparent(img):
    """
    Convert black background to transparent, white content stays white.
    """
    img = img.convert("RGBA")
    pixels = img.load()
    width, height = img.size
    
    for y in range(height):
        for x in range(width):
            r, g, b, a = pixels[x, y]
            brightness = (r + g + b) / 3
            
            if brightness < 50:
                # Dark pixel -> transparent
                #
                pixels[x, y] = (255, 255, 255, 0)
            else:
                # Light pixel -> white with alpha based on brightness
                #
                alpha = int(min(255, brightness * 2))
                pixels[x, y] = (255, 255, 255, alpha)
    
    return img


def main():
    script_dir = os.path.dirname(os.path.abspath(__file__))
    project_root = os.path.dirname(script_dir)
    
    source_path = os.path.join(
        project_root, 
        "JUCE/SmartGridOne/Assets/ChatGPTBlackOnWhiteModBadges2.png"
    )
    output_path = os.path.join(
        project_root, 
        "JUCE/SmartGridOne/Assets/ModBadgesTransparent2.png"
    )
    
    # Load source image
    #
    img = Image.open(source_path)
    width, height = img.size
    print(f"Source image size: {width}x{height}")
    
    # Convert to RGBA if needed
    #
    if img.mode != "RGBA":
        img = img.convert("RGBA")
    
    # Convert black to transparent
    #
    img = convert_to_white_on_transparent(img)
    
    # Save
    #
    img.save(output_path, "PNG")
    print(f"Saved: {output_path}")
    
    print("\nDone! You can now manually crop the glyphs.")

if __name__ == "__main__":
    main()

