Import("env")

# Get the current list of extra images to be flashed
current_flash_extra_images = env.get("FLASH_EXTRA_IMAGES", [])
print("Current FLASH_EXTRA_IMAGES before modification:")
if not current_flash_extra_images:
    print("  FLASH_EXTRA_IMAGES is empty or not defined.")
else:
    for i, item in enumerate(current_flash_extra_images):
        print(f"  Item {i}: {item}")

new_flash_extra_images = []
found_and_removed_tinyuf2 = False

for item in current_flash_extra_images:
    # FLASH_EXTRA_IMAGES is typically a list of (offset, path) tuples for ESP32
    path_to_check = ""
    if isinstance(item, tuple) and len(item) == 2:
        path_to_check = item[1]
    elif isinstance(item, str): # Fallback, less common for ESP32
        path_to_check = item
    
    if "tinyuf2.bin" not in path_to_check.lower(): # case-insensitive check for safety
        new_flash_extra_images.append(item)
    else:
        print(f"Info: Identified and removing '{path_to_check}' from FLASH_EXTRA_IMAGES.")
        found_and_removed_tinyuf2 = True

if found_and_removed_tinyuf2:
    env.Replace(FLASH_EXTRA_IMAGES=new_flash_extra_images)
    print("FLASH_EXTRA_IMAGES after modification:")
    if not new_flash_extra_images:
        print("  FLASH_EXTRA_IMAGES is now empty.")
    else:
        for i, item in enumerate(new_flash_extra_images):
            print(f"  Item {i}: {item}")
else:
    print("Info: tinyuf2.bin was not found in FLASH_EXTRA_IMAGES. No changes made to this list by the script.") 