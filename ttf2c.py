#!/usr/bin/env python3
import os
import glob
import re

def safe_font_name(filename):
    """Convert filename to safe C variable name"""
    # Remove file extension and path
    name = os.path.splitext(os.path.basename(filename))[0]
    # Replace non-alphanumeric with underscore
    name = re.sub(r'[^a-zA-Z0-9]', '_', name)
    return name

def ttf_to_header(ttf_file, output_dir):
    """Convert TTF file to C header"""
    with open(ttf_file, 'rb') as f:
        font_data = f.read()
    
    font_name = safe_font_name(ttf_file)
    output_file = os.path.join(output_dir, f"{font_name}.h")
    
    # Calculate size
    size = len(font_data)
    
    # Format data as C array
    hex_array = []
    for i, byte in enumerate(font_data):
        if i % 12 == 0:
            hex_array.append("\n  ")
        hex_array.append(f"0x{byte:02x}, ")
    
    font_data_str = "".join(hex_array)
    
    # Create header content - use FLASH_ATTR for ESP32 to store in flash memory
    header_content = f"""/**
 * @file {font_name}.h
 * @brief TTF font for TinyTTF
 * Generated from {os.path.basename(ttf_file)}
 */

#ifndef {font_name.upper()}_H
#define {font_name.upper()}_H

#include <stdint.h>

/**
 * {os.path.basename(ttf_file)} font data
 * Stored in flash memory to save RAM
 */
const uint8_t {font_name}_data[] = {{{font_data_str}
}};

/**
 * Font data size
 */
const uint32_t {font_name}_size = {size};

#endif /* {font_name.upper()}_H */
"""
    
    # Write to file
    with open(output_file, 'w') as f:
        f.write(header_content)
    
    print(f"Generated {output_file} from {ttf_file}")
    return True

def main():
    # Create output directory if it doesn't exist
    output_dir = "include/fonts"
    os.makedirs(output_dir, exist_ok=True)
    
    # Process all TTF files in typography directory
    ttf_files = glob.glob("typography/*.ttf")
    if not ttf_files:
        print("No TTF files found in typography directory")
        return
    
    for ttf_file in ttf_files:
        ttf_to_header(ttf_file, output_dir)
    
    # Generate fonts.h file that includes all font headers
    headers = [safe_font_name(f) for f in ttf_files]
    fonts_h_content = f"""/**
 * @file fonts.h
 * @brief Includes all font headers
 */

#ifndef FONTS_H
#define FONTS_H

// Include all font headers
"""
    for header in headers:
        fonts_h_content += f'#include "{header}.h"\n'
    
    fonts_h_content += "\n#endif /* FONTS_H */\n"
    
    with open(os.path.join(output_dir, "fonts.h"), 'w') as f:
        f.write(fonts_h_content)
    
    print(f"Generated {output_dir}/fonts.h")

if __name__ == "__main__":
    main() 