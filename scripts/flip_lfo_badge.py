#!/usr/bin/env python3

from PIL import Image
import sys
import os

# Get the script directory and project root
script_dir = os.path.dirname(os.path.abspath(__file__))
project_root = os.path.dirname(script_dir)
lfo_path = os.path.join(project_root, "JUCE", "SmartGridOne", "Assets", "mod_badge_LFO.png")

if not os.path.exists(lfo_path):
    print(f"Error: LFO badge image not found at {lfo_path}")
    sys.exit(1)

# Load the image
img = Image.open(lfo_path)

# Flip vertically
flipped_img = img.transpose(Image.Transpose.FLIP_TOP_BOTTOM)

# Save back to the same file
flipped_img.save(lfo_path)
print(f"Successfully flipped LFO badge image: {lfo_path}")

