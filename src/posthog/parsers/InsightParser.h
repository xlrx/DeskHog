#ifndef INSIGHT_PARSER_H
#define INSIGHT_PARSER_H

#define ARDUINOJSON_DEFAULT_NESTING_LIMIT 50
#include <ArduinoJson.h>

class InsightParser {
public:
    enum class InsightType {
        NUMERIC_CARD,
        LINE_GRAPH,
        AREA_CHART,
        FUNNEL,
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
    
    // Funnel-specific methods
    bool hasFunnelStructure() const;
    size_t getFunnelBreakdownCount() const;
    size_t getFunnelStepCount() const;
    
    bool getFunnelStepData(
        size_t breakdown_index,
        size_t step_index,
        char* name_buffer,
        size_t name_buffer_size,
        uint32_t* count,
        double* conversion_time_avg,
        double* conversion_time_median
    );
    
    bool getFunnelBreakdownName(
        size_t breakdown_index,
        char* buffer,
        size_t buffer_size
    );
    
    bool getFunnelTotalCounts(
        size_t breakdown_index,
        uint32_t* counts_array,
        double* conversion_rates
    );
    
    bool getFunnelConversionTimes(
        size_t breakdown_index,
        size_t step_index,
        double* avg_time,
        double* median_time
    );
    
    bool getFunnelStepMetadata(
        size_t step_index,
        char* custom_name_buffer,
        size_t name_buffer_size,
        char* action_id_buffer,
        size_t action_buffer_size
    );
    
    bool getFunnelBreakdownComparison(
        size_t step_index,
        uint32_t* counts,
        double* conversion_rates
    );
    
    bool getFunnelTimeWindow(uint32_t* window_days);
    
private:
    // Helper methods for insight type detection
    bool hasNumericCardStructure() const;
    bool hasLineGraphStructure() const;
    bool hasAreaChartStructure() const;

    DynamicJsonDocument doc;  // Changed from StaticJsonDocument to DynamicJsonDocument
    bool valid;
};

#endif // INSIGHT_PARSER_H 