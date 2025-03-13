#include "InsightNumericParser.h"
#include <stdio.h>

InsightNumericParser::InsightNumericParser(const char* json) : valid(false) {
    DeserializationError error = deserializeJson(doc, json);
    if (error) {
        printf("JSON Deserialization failed: %s\n", error.c_str());
        return;
    }
    valid = true;
}

bool InsightNumericParser::getName(char* buffer, size_t bufferSize) {
    if (!valid || bufferSize == 0) {
        return false;
    }
    
    const char* name = doc["results"][0]["name"];
    if (!name) {
        return false;
    }
    
    strncpy(buffer, name, bufferSize - 1);
    buffer[bufferSize - 1] = '\0';
    return true;
}

double InsightNumericParser::getValue() {
    if (!valid) {
        return 0.0;
    }
    
    // Get the first result's aggregated_value
    return doc["results"][0]["result"][0]["aggregated_value"] | 0.0;
}

bool InsightNumericParser::isValid() const {
    return valid;
} 