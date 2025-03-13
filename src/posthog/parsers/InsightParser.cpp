#include "InsightParser.h"
#include <stdio.h>

InsightParser::InsightParser(const char* json) : doc(65536), valid(false) {
    DeserializationError error = deserializeJson(doc, json);
    if (error) {
        printf("JSON Deserialization failed: %s\n", error.c_str());
        return;
    }
    valid = true;
}

bool InsightParser::getName(char* buffer, size_t bufferSize) {
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

double InsightParser::getNumericCardValue() {
    if (!valid) {
        return 0.0;
    }
    
    // Get the first result's aggregated_value
    return doc["results"][0]["result"][0]["aggregated_value"] | 0.0;
}

bool InsightParser::isValid() const {
    return valid;
}

bool InsightParser::hasNumericCardStructure() const {
    if (!valid) return false;
    
    // Check if we have the basic numeric card structure:
    // - results array exists
    // - first result has an aggregated_value
    JsonArrayConst results = doc["results"];
    if (results.isNull() || results.size() == 0) return false;
    
    JsonArrayConst result = results[0]["result"];
    if (result.isNull() || result.size() == 0) return false;
    
    return !result[0]["aggregated_value"].isNull();
}

bool InsightParser::hasLineGraphStructure() const {
    if (!valid) return false;
    
    // Check for line graph structure:
    // - results array exists
    // - first result has result array with multiple points
    // - optionally check for ActionsLineGraph display type
    JsonArrayConst results = doc["results"];
    if (results.isNull() || results.size() == 0) return false;
    
    JsonArrayConst timeseriesData = results[0]["result"];
    if (timeseriesData.isNull() || timeseriesData.size() <= 1) return false;
    
    // Additional check: verify it's explicitly a line graph if display type is present
    const char* displayType = results[0]["query"]["display"];
    if (displayType && strcmp(displayType, "ActionsLineGraph") == 0) {
        return true;
    }
    
    // If no display type specified, having time series data is sufficient
    return true;
}

bool InsightParser::hasAreaChartStructure() const {
    if (!valid) return false;
    
    // Area charts are similar to line graphs but typically have
    // an additional "compare" property
    JsonArrayConst results = doc["results"];
    if (results.isNull() || results.size() == 0) return false;
    
    return !results[0]["compare"].isNull();
}

InsightParser::InsightType InsightParser::detectInsightType() const {
    if (!valid) return InsightType::INSIGHT_NOT_SUPPORTED;
    
    // Check each type in order of specificity
    if (hasNumericCardStructure()) return InsightType::NUMERIC_CARD;
    if (hasLineGraphStructure()) return InsightType::LINE_GRAPH;
    if (hasAreaChartStructure()) return InsightType::AREA_CHART;
    
    return InsightType::INSIGHT_NOT_SUPPORTED;
}

size_t InsightParser::getSeriesPointCount() const {
    if (!valid || !hasLineGraphStructure()) return 0;
    
    JsonArrayConst results = doc["results"];
    JsonArrayConst timeseriesData = results[0]["result"];
    return timeseriesData.size();
}

bool InsightParser::getSeriesYValues(double* yValues, size_t maxPoints) const {
    if (!valid || !hasLineGraphStructure() || !yValues || maxPoints == 0) return false;
    
    JsonArrayConst results = doc["results"];
    JsonArrayConst timeseriesData = results[0]["result"];
    size_t pointCount = timeseriesData.size();
    size_t points = (pointCount < maxPoints) ? pointCount : maxPoints;
    
    for (size_t i = 0; i < points; i++) {
        yValues[i] = timeseriesData[i][1].as<double>();
    }
    
    return true;
}

bool InsightParser::getSeriesXLabel(size_t index, char* buffer, size_t bufferSize) const {
    if (!valid || !hasLineGraphStructure() || !buffer || bufferSize == 0) return false;
    
    JsonArrayConst results = doc["results"];
    JsonArrayConst timeseriesData = results[0]["result"];
    
    if (index >= timeseriesData.size()) return false;
    
    const char* dateStr = timeseriesData[index][0];
    if (!dateStr) return false;
    
    // Copy just the year and month (YYYY-MM) to keep labels compact
    size_t len = strlen(dateStr);
    if (len < 7) return false;  // Ensure we have at least YYYY-MM
    
    size_t copyLen = (7 < bufferSize) ? 7 : bufferSize - 1;
    strncpy(buffer, dateStr, copyLen);
    buffer[copyLen] = '\0';
    
    return true;
}

void InsightParser::getSeriesRange(double* minValue, double* maxValue) const {
    if (!valid || !hasLineGraphStructure() || !minValue || !maxValue) {
        if (minValue) *minValue = 0.0;
        if (maxValue) *maxValue = 0.0;
        return;
    }
    
    JsonArrayConst results = doc["results"];
    JsonArrayConst timeseriesData = results[0]["result"];
    
    *minValue = timeseriesData[0][1].as<double>();
    *maxValue = *minValue;
    
    for (size_t i = 1; i < timeseriesData.size(); i++) {
        double value = timeseriesData[i][1].as<double>();
        if (value < *minValue) *minValue = value;
        if (value > *maxValue) *maxValue = value;
    }
} 