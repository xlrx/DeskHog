#!/usr/bin/env python3
import os
import sys
import glob

# Check if PIL (Pillow) is installed
try:
    from PIL import Image
    import numpy as np
    HAS_PIL = True
except ImportError:
    HAS_PIL = False

def png_to_c_array(png_file, output_dir, var_name_override=None):
    """Convert PNG file to LVGL compatible C array"""
    if not HAS_PIL:
        print(f"Skipping {png_file} - PIL (Pillow) and numpy are required")
        return None
    
    # Create a safe C variable name from the filename
    base_name = os.path.splitext(os.path.basename(png_file))[0]
    var_name = var_name_override if var_name_override else base_name.replace('-', '_').replace('.', '_')
    
    # Load the image
    img = Image.open(png_file)
    width, height = img.size
    
    # Convert to RGBA if not already
    if img.mode != 'RGBA':
        img = img.convert('RGBA')
    
    # Get pixel data as a numpy array
    pixels = np.array(img)
    
    # Create output directory if it doesn't exist
    os.makedirs(output_dir, exist_ok=True)
    
    # Get all PNG files in sequence to determine array indices
    output_c_file = os.path.join(output_dir, f"sprite_{var_name}.c")
    output_h_file = os.path.join(output_dir, f"sprite_{var_name}.h")
    
    # Write C file with pixel data
    with open(output_c_file, 'w') as f:
        f.write(f"""/**
 * @file sprite_{var_name}.c
 * @brief LVGL sprite generated from {os.path.basename(png_file)}
 */

#include "lvgl.h"
#include "sprite_{var_name}.h"

// Image data for {os.path.basename(png_file)}
static const uint8_t sprite_{var_name}_map[] = {{
""")
        
        # Process pixels
        for y in range(height):
            f.write("    ")
            for x in range(width):
                r, g, b, a = pixels[y, x]
                
                # Using BGRA order for LVGL ARGB8888 format (this works!)
                # Each component is 8 bits (1 byte)
                
                # Output as individual bytes in BGRA order
                f.write(f"{b}, {g}, {r}, {a},")
                
                if x < width - 1:
                    f.write(" ")
            f.write("\n")
        
        f.write("};\n\n")
        
        # LVGL image descriptor - updated for LVGL 9.x
        f.write(f"""// LVGL image descriptor
const lv_img_dsc_t sprite_{var_name} = {{
    .header = {{
        .cf = LV_COLOR_FORMAT_ARGB8888,
        .w = {width},
        .h = {height},
    }},
    .data_size = {width * height * 4},
    .data = sprite_{var_name}_map
}};
""")
    
    # Write header file
    with open(output_h_file, 'w') as f:
        f.write(f"""/**
 * @file sprite_{var_name}.h
 * @brief LVGL sprite generated from {os.path.basename(png_file)}
 */

#pragma once

#ifdef __cplusplus
extern "C" {{
#endif

#include "lvgl.h"

// Image descriptor declaration
extern const lv_img_dsc_t sprite_{var_name};

#ifdef __cplusplus
}}
#endif
""")
    
    print(f"Generated {output_c_file} and {output_h_file}")
    return var_name

def create_dummy_sprite_files(output_dir, file_names):
    """Create dummy sprite files when PIL is not installed"""
    os.makedirs(output_dir, exist_ok=True)
    sprite_names = []
    
    for file_name in file_names:
        # Create a safe C variable name from the filename
        base_name = os.path.splitext(os.path.basename(file_name))[0]
        var_name = base_name.replace('-', '_').replace('.', '_')
        sprite_names.append(var_name)
        
        # Create placeholder header file
        output_h_file = os.path.join(output_dir, f"sprite_{var_name}.h")
        with open(output_h_file, 'w') as f:
            f.write(f"""/**
 * @file sprite_{var_name}.h
 * @brief LVGL sprite placeholder for {os.path.basename(file_name)}
 * This is a dummy sprite because PIL was not installed
 */

#pragma once

#ifdef __cplusplus
extern "C" {{
#endif

#include "lvgl.h"

// Dummy image descriptor declaration
extern const lv_img_dsc_t sprite_{var_name};

#ifdef __cplusplus
}}
#endif
""")
        
        # Create placeholder C file with updated LVGL 9.x descriptor format
        output_c_file = os.path.join(output_dir, f"sprite_{var_name}.c")
        with open(output_c_file, 'w') as f:
            f.write(f"""/**
 * @file sprite_{var_name}.c
 * @brief LVGL sprite placeholder for {os.path.basename(file_name)}
 * This is a dummy sprite because PIL was not installed
 */

#include "lvgl.h"
#include "sprite_{var_name}.h"

// Dummy 16x16 image data (just a colored square)
static const uint8_t sprite_{var_name}_map[] = {{
    // 16x16 red square with alpha in BGRA format
    0x00, 0x00, 0xFF, 0xFF,  0x00, 0x00, 0xFF, 0xFF,  0x00, 0x00, 0xFF, 0xFF,
    0x00, 0x00, 0xFF, 0xFF,  0x00, 0x00, 0xFF, 0xFF,  0x00, 0x00, 0xFF, 0xFF,
    0x00, 0x00, 0xFF, 0xFF,  0x00, 0x00, 0xFF, 0xFF,  0x00, 0x00, 0xFF, 0xFF,
    0x00, 0x00, 0xFF, 0xFF,  0x00, 0x00, 0xFF, 0xFF,  0x00, 0x00, 0xFF, 0xFF
}};

// LVGL image descriptor - updated for LVGL 9.x
const lv_img_dsc_t sprite_{var_name} = {{
    .header = {{
        .cf = LV_COLOR_FORMAT_ARGB8888,
        .w = 16,
        .h = 16,
    }},
    .data_size = 16 * 16 * 4,
    .data = sprite_{var_name}_map
}};
""")
        
        print(f"Generated placeholder {output_c_file} and {output_h_file}")
    
    return sprite_names

def main():
    print("PNG to LVGL Sprite Converter")
    print("============================")
    
    # Check if PIL is installed and warn if not
    if not HAS_PIL:
        print("\nWARNING: Python Imaging Library (PIL/Pillow) and numpy are not installed.")
        print("The script will create placeholder sprites instead of converting real images.")
        print("\nTo install the required libraries, run:")
        print("  python3 -m pip install --user pillow numpy")
        print("or create a virtual environment:")
        print("  python3 -m venv .venv")
        print("  source .venv/bin/activate")
        print("  pip install pillow numpy")
        print("\nProceeding with placeholder sprites...\n")
    
    # Create output directory if it doesn't exist
    output_dir = "include/sprites"
    os.makedirs(output_dir, exist_ok=True)
    
    # Find all PNG files in the sprites/walking directory
    png_files = sorted(glob.glob("sprites/walking/*.png"))
    
    if not png_files:
        print("No PNG files found in sprites/walking directory!")
        sys.exit(1)
    
    print(f"Found {len(png_files)} PNG files to process")
    
    # Process each PNG file or create dummy files if PIL is not installed
    sprite_names = []
    if HAS_PIL:
        for png_file in png_files:
            var_name = png_to_c_array(png_file, output_dir)
            if var_name:
                sprite_names.append(var_name)
    else:
        sprite_names = create_dummy_sprite_files(output_dir, png_files)
    
    # Generate sprites.h file that includes all sprite headers
    sprites_h_content = """/**
 * @file sprites.h
 * @brief Includes all LVGL sprite headers
 */

#ifndef LVGL_SPRITES_H
#define LVGL_SPRITES_H

#ifdef __cplusplus
extern "C" {
#endif

// Include all sprite headers
"""
    for sprite_name in sprite_names:
        sprites_h_content += f'#include "sprite_{sprite_name}.h"\n'
    
    # Add array of all sprites
    sprites_h_content += """
// Array of all walking animation sprites
extern const lv_img_dsc_t* walking_sprites[];
extern const uint8_t walking_sprites_count;

#ifdef __cplusplus
}
#endif

#endif /* LVGL_SPRITES_H */
"""
    
    with open(os.path.join(output_dir, "sprites.h"), 'w') as f:
        f.write(sprites_h_content)
    
    # Generate sprites.c file with the sprite array
    sprites_c_content = """/**
 * @file sprites.c
 * @brief Contains arrays of sprite pointers
 */

#include "sprites.h"

// Array of all walking animation sprites
const lv_img_dsc_t* walking_sprites[] = {
"""
    
    for sprite_name in sprite_names:
        sprites_c_content += f'    &sprite_{sprite_name},\n'
    
    sprites_c_content += """};

// Number of sprites in the walking animation
const uint8_t walking_sprites_count = sizeof(walking_sprites) / sizeof(walking_sprites[0]);
"""
    
    with open(os.path.join(output_dir, "sprites.c"), 'w') as f:
        f.write(sprites_c_content)
    
    print(f"Generated {output_dir}/sprites.h and {output_dir}/sprites.c")
    print(f"Successfully processed {len(sprite_names)} sprites")

if __name__ == "__main__":
    main() 