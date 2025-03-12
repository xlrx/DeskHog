#!/usr/bin/env python3
print("DEBUG: htmlconvert.py is being loaded!")
import os
import re

# Configuration
html_dir = "html"
html_file = f"{html_dir}/portal.html"  # The main HTML file
css_file = f"{html_dir}/portal.css"    # The CSS file
js_file = f"{html_dir}/portal.js"      # The JavaScript file
output_dir = "include"                 # Output directory for header file
header_name = "html_portal.h"          # Name for the header file

def escape_for_cpp(text):
    """Escape text for C++ string literals"""
    text = text.replace('\\', '\\\\')
    text = text.replace('"', '\\"')
    text = text.replace('\n', '\\n"\n"')
    return text

def html_to_header():
    """Convert the HTML file to a C++ header file, inlining CSS and JS"""
    print("DEBUG: Starting html_to_header conversion")
    
    # Read HTML file
    try:
        with open(html_file, 'r', encoding='utf-8') as f:
            html_content = f.read()
        print(f"DEBUG: Successfully read {html_file}")
    except Exception as e:
        print(f"DEBUG: Error reading HTML file: {e}")
        return
    
    # Read CSS file
    try:
        with open(css_file, 'r', encoding='utf-8') as f:
            css_content = f.read()
        print(f"DEBUG: Successfully read {css_file}")
    except Exception as e:
        print(f"DEBUG: CSS file not found, skipping inline: {e}")
        css_content = None
    
    # Read JavaScript file
    try:
        with open(js_file, 'r', encoding='utf-8') as f:
            js_content = f.read()
        print(f"DEBUG: Successfully read {js_file}")
    except Exception as e:
        print(f"DEBUG: JS file not found, skipping inline: {e}")
        js_content = None
    
    # Inline CSS if found
    if css_content:
        html_content = html_content.replace(
            '<link rel="stylesheet" href="portal.css">',
            f'<style>\n{css_content}\n</style>'
        )
    
    # Inline JavaScript if found
    if js_content:
        html_content = html_content.replace(
            '<script src="portal.js"></script>',
            f'<script>\n{js_content}\n</script>'
        )
    
    # Create output directory if it doesn't exist
    try:
        os.makedirs(output_dir, exist_ok=True)
        print(f"DEBUG: Ensured directory {output_dir} exists")
    except Exception as e:
        print(f"DEBUG: Error creating directory: {e}")
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
    
    # Write header file
    try:
        header_path = os.path.join(output_dir, header_name)
        with open(header_path, 'w', encoding='utf-8') as f:
            f.write(header_content)
        print(f"DEBUG: Successfully generated {header_path}")
        print(f"DEBUG: Final HTML size: {len(html_content)} bytes")
        print(f"DEBUG: Generated header size: {os.path.getsize(header_path)} bytes")
    except Exception as e:
        print(f"DEBUG: Error writing header file: {e}")

# Run the conversion
print("DEBUG: About to convert HTML")
Import("env")
html_to_header()