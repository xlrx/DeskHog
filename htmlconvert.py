#!/usr/bin/env python3
print("DEBUG: htmlconvert.py is being loaded!")
import os
import re
Import("env")

# Configuration
html_file = "html/portal.html"  # The single HTML file
output_dir = "include"  # Output directory for header file
header_name = "html_portal.h"  # Name for the header file

def escape_for_cpp(text):
    """Escape text for C++ string literals"""
    text = text.replace('\\', '\\\\')
    text = text.replace('"', '\\"')
    text = text.replace('\n', '\\n"\n"')
    return text

def html_to_header():
    """Convert the HTML file to a C++ header file"""
    print("DEBUG: Starting html_to_header conversion")
    # Read HTML file
    try:
        with open(html_file, 'r', encoding='utf-8') as f:
            html_content = f.read()
            print(f"DEBUG: Successfully read {html_file}")
    except Exception as e:
        print(f"DEBUG: Error reading HTML file: {e}")
        return
    
    # Create header content
    header_content = f"""// Generated file - do not edit!
// Source: {html_file}

#ifndef GENERATED_HTML_PORTAL_H
#define GENERATED_HTML_PORTAL_H

#include <pgmspace.h>

static const char PORTAL_HTML[] PROGMEM = "{escape_for_cpp(html_content)}";

#endif // GENERATED_HTML_PORTAL_H
"""
    
    # Create output directory if it doesn't exist
    try:
        os.makedirs(output_dir, exist_ok=True)
        print(f"DEBUG: Ensured directory {output_dir} exists")
    except Exception as e:
        print(f"DEBUG: Error creating directory: {e}")
        return
    
    # Write header file
    try:
        header_path = os.path.join(output_dir, header_name)
        with open(header_path, 'w', encoding='utf-8') as f:
            f.write(header_content)
        print(f"DEBUG: Successfully generated {header_path} from {html_file}")
    except Exception as e:
        print(f"DEBUG: Error writing header file: {e}")

print("DEBUG: About to register callback")
html_to_header()