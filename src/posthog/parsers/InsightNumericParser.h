#pragma once

#include <ArduinoJson.h>

class InsightNumericParser {
public:
    InsightNumericParser(const char* json);
    
    // Get the insight name (e.g., "Active Viewers")
    bool getName(char* buffer, size_t bufferSize);
    
    // Get the numeric value
    double getValue();
    
    // Check if parsing was successful
    bool isValid() const;
    
private:
    StaticJsonDocument<65536> doc;  // 64KB for larger JSON responses
    bool valid;
}; 