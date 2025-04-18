#pragma once

#include <Arduino.h>

class NumberFormat {
public:
    // Format a number with thousands separators
    static void addThousandsSeparators(char* buffer, size_t bufferSize, uint32_t number) {
        // Convert to string first to get length
        char tempBuffer[16];
        snprintf(tempBuffer, sizeof(tempBuffer), "%u", number);
        size_t len = strlen(tempBuffer);
        
        // Calculate how many thousands separators we'll need
        size_t separatorCount = (len - 1) / 3;
        
        // Check if we have enough space in the buffer
        if (len + separatorCount + 1 > bufferSize) {
            // If not enough space, just copy without separators
            strncpy(buffer, tempBuffer, bufferSize);
            buffer[bufferSize - 1] = '\0';
            return;
        }
        
        // Add thousands separators
        size_t sourceIndex = len;
        size_t targetIndex = len + separatorCount;
        buffer[targetIndex] = '\0';
        
        int digitCount = 0;
        while (sourceIndex > 0) {
            sourceIndex--;
            if (digitCount > 0 && digitCount % 3 == 0) {
                targetIndex--;
                buffer[targetIndex] = ',';
            }
            targetIndex--;
            buffer[targetIndex] = tempBuffer[sourceIndex];
            digitCount++;
        }
    }

private:
    // Private constructor to prevent instantiation
    NumberFormat() {}
}; 