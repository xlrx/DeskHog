#pragma once

#include <ArduinoJson.h>

class InsightParser {
public:
    enum class InsightType {
        NUMERIC_CARD,
        LINE_GRAPH,
        AREA_CHART,
        INSIGHT_NOT_SUPPORTED
    };

    InsightParser(const char* json);
    ~InsightParser() = default;  // Explicitly declare destructor
    
    // Delete copy constructor and assignment operator
    InsightParser(const InsightParser&) = delete;
    InsightParser& operator=(const InsightParser&) = delete;
    
    // Get the insight name (e.g., "Active Viewers")
    bool getName(char* buffer, size_t bufferSize);
    
    // Get the numeric value for numeric cards
    double getNumericCardValue();
    
    // Line Graph Data Accessors
    size_t getSeriesPointCount() const;  // Get number of data points
    bool getSeriesYValues(double* yValues) const;  // Fill array with y-values
    bool getSeriesXLabel(size_t index, char* buffer, size_t bufferSize) const;  // Get x-axis label for point
    void getSeriesRange(double* minValue, double* maxValue) const;  // Get y-value range for scaling
    
    // Check if parsing was successful
    bool isValid() const;

    // Detect the type of insight from the JSON structure
    InsightType detectInsightType() const;
    
private:
    // Helper methods for insight type detection
    bool hasNumericCardStructure() const;
    bool hasLineGraphStructure() const;
    bool hasAreaChartStructure() const;

    DynamicJsonDocument doc;  // Changed from StaticJsonDocument to DynamicJsonDocument
    bool valid;
}; 