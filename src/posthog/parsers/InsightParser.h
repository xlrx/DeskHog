#pragma once

#define ARDUINOJSON_DEFAULT_NESTING_LIMIT 50
#include <ArduinoJson.h>

/**
 * @class InsightParser
 * @brief Parser for PostHog insight data with support for multiple visualization types
 * 
 * Handles parsing and data extraction from PostHog insight JSON responses.
 * Supports multiple visualization types including:
 * - Numeric cards (single value displays)
 * - Line graphs (time series data)
 * - Area charts (with comparison data)
 * - Funnels (with optional breakdowns and conversion metrics)
 * 
 * Features:
 * - Memory-efficient JSON parsing using ArduinoJson
 * - Support for PSRAM when available on ESP32 platforms
 * - Thread-safe design
 * - Automatic insight type detection
 * - Comprehensive funnel analysis including:
 *   - Multiple breakdown support (up to MAX_BREAKDOWNS)
 *   - Conversion time analysis
 *   - Step metadata extraction
 *   - Time window configuration
 */
class InsightParser {
public:
    /**
     * @enum InsightType
     * @brief Supported visualization types for insights
     */
    enum class InsightType {
        NUMERIC_CARD,         ///< Single value display (e.g. total users)
        LINE_GRAPH,          ///< Time series line graph with date-based X-axis
        AREA_CHART,          ///< Area chart visualization with comparison data
        FUNNEL,              ///< Funnel visualization with steps and conversion metrics
        INSIGHT_NOT_SUPPORTED ///< Unsupported or unrecognized insight type
    };

    /**
     * @brief Constructor - parses JSON data
     * @param json Raw JSON string to parse
     * 
     * Initializes parser with JSON data and attempts to allocate memory.
     * On ESP32 platforms, will attempt to use PSRAM if available.
     * Use isValid() to check if parsing was successful.
     */
    InsightParser(const char* json);

    /**
     * @brief Default destructor
     */
    ~InsightParser() = default;
    
    // Delete copy constructor and assignment operator to prevent copying
    InsightParser(const InsightParser&) = delete;
    InsightParser& operator=(const InsightParser&) = delete;
    
    /**
     * @brief Get insight name/title
     * @param buffer Buffer to store name
     * @param bufferSize Size of buffer
     * @return true if name was retrieved successfully
     */
    bool getName(char* buffer, size_t bufferSize);
    
    /**
     * @brief Get numeric value for single-value insights
     * @return The numeric value or 0.0 if not applicable/available
     * 
     * Returns the aggregated_value from the first result for numeric card insights.
     * Should only be called if detectInsightType() returns NUMERIC_CARD.
     */
    double getNumericCardValue();
    
    /**
     * @brief Get number of data points in line graph series
     * @return Number of points or 0 if not a line graph
     * 
     * Returns the number of time series data points for line graphs.
     * Should only be called if detectInsightType() returns LINE_GRAPH.
     */
    size_t getSeriesPointCount() const;

    /**
     * @brief Get Y-values for line graph series
     * @param yValues Array to fill with Y-values (must be pre-allocated)
     * @return true if values were retrieved successfully
     * 
     * Populates the provided array with Y-values from the time series.
     * Array size must match getSeriesPointCount().
     */
    bool getSeriesYValues(double* yValues) const;

    /**
     * @brief Get X-axis label for a data point
     * @param index Point index
     * @param buffer Buffer to store label
     * @param bufferSize Size of buffer
     * @return true if label was retrieved successfully
     * 
     * Retrieves the date string (YYYY-MM format) for the specified data point.
     * Buffer must be at least 8 bytes to store YYYY-MM\0.
     */
    bool getSeriesXLabel(size_t index, char* buffer, size_t bufferSize) const;

    /**
     * @brief Get Y-value range for scaling
     * @param minValue Pointer to store minimum value
     * @param maxValue Pointer to store maximum value
     * 
     * Calculates the min/max Y values across all data points.
     * Useful for scaling visualizations appropriately.
     */
    void getSeriesRange(double* minValue, double* maxValue) const;
    
    /**
     * @brief Check if parsing was successful
     * @return true if JSON was parsed successfully
     * 
     * Verifies if the JSON data was valid and successfully parsed.
     * Should be called before attempting to use any other methods.
     */
    bool isValid() const;

    /**
     * @brief Determine visualization type from JSON structure
     * @return Detected InsightType
     * 
     * Analyzes the JSON structure to determine the appropriate visualization type.
     * Should be called before using type-specific methods.
     */
    InsightType detectInsightType() const;
    
    // Funnel-specific methods
    /**
     * @brief Check if JSON has funnel structure
     * @return true if basic funnel structure exists
     * 
     * Verifies if the insight is explicitly marked as a FUNNELS type.
     */
    bool hasFunnelStructure() const;

    /**
     * @brief Check if funnel has result data
     * @return true if funnel contains result data
     * 
     * Checks if the funnel has populated data vs just configuration.
     * Unpopulated funnels will only have filter/metadata information.
     */
    bool hasFunnelResultData() const;

    /**
     * @brief Check if funnel has nested breakdown structure
     * @return true if funnel has breakdown data
     * 
     * Determines if the funnel uses a nested structure with breakdowns
     * vs a flat single-series structure.
     */
    bool hasFunnelNestedStructure() const;

    /**
     * @brief Get number of funnel breakdowns
     * @return Number of breakdowns or 1 if no breakdowns
     * 
     * Returns the number of breakdown series in the funnel.
     * Limited by MAX_BREAKDOWNS (5).
     */
    size_t getFunnelBreakdownCount() const;

    /**
     * @brief Get number of steps in funnel
     * @return Number of funnel steps
     * 
     * Returns total number of steps across all events/actions.
     * Works for both populated and unpopulated funnels.
     */
    size_t getFunnelStepCount() const;
    
    /**
     * @brief Get data for a specific funnel step
     * 
     * @param breakdown_index Index of breakdown to get data for
     * @param step_index Index of step within breakdown
     * @param name_buffer Buffer to store step name
     * @param name_buffer_size Size of name buffer
     * @param count Pointer to store step count
     * @param conversion_time_avg Pointer to store average conversion time
     * @param conversion_time_median Pointer to store median conversion time
     * @return true if data was retrieved successfully
     * 
     * Retrieves comprehensive data about a specific step in a funnel breakdown.
     * Any parameter except indices can be null if that data isn't needed.
     */
    bool getFunnelStepData(
        size_t breakdown_index,
        size_t step_index,
        char* name_buffer,
        size_t name_buffer_size,
        uint32_t* count,
        double* conversion_time_avg,
        double* conversion_time_median
    );
    
    /**
     * @brief Get name of a funnel breakdown
     * 
     * @param breakdown_index Index of breakdown
     * @param buffer Buffer to store name
     * @param buffer_size Size of buffer
     * @return true if name was retrieved successfully
     * 
     * Gets the identifying name for a specific breakdown series.
     * Returns "All users" for flat (non-breakdown) funnels.
     */
    bool getFunnelBreakdownName(
        size_t breakdown_index,
        char* buffer,
        size_t buffer_size
    );
    
    /**
     * @brief Get total counts and conversion rates for all steps
     * 
     * @param breakdown_index Index of breakdown to analyze
     * @param counts Array to store step counts (must fit getFunnelStepCount())
     * @param conversion_rates Array to store conversion rates (optional)
     * @return true if data was retrieved successfully
     * 
     * Retrieves the total user count for each step and optionally calculates
     * step-to-step conversion rates. For nested structures, sums across breakdowns.
     */
    bool getFunnelTotalCounts(
        size_t breakdown_index,
        uint32_t* counts,
        double* conversion_rates
    );
    
    /**
     * @brief Get conversion times for a specific step
     * 
     * @param breakdown_index Index of breakdown
     * @param step_index Index of step (must be > 0)
     * @param avg_time Pointer to store average conversion time
     * @param median_time Pointer to store median conversion time
     * @return true if times were retrieved successfully
     * 
     * Gets the average and median conversion times from previous step.
     * Not applicable to first step (step_index 0).
     */
    bool getFunnelConversionTimes(
        size_t breakdown_index,
        size_t step_index,
        double* avg_time,
        double* median_time
    );
    
    /**
     * @brief Get metadata about a funnel step
     * 
     * @param step_index Index of step
     * @param custom_name_buffer Buffer for custom step name
     * @param name_buffer_size Size of name buffer
     * @param action_id_buffer Buffer for action/event ID
     * @param action_buffer_size Size of action buffer
     * @return true if metadata was retrieved successfully
     * 
     * Retrieves configuration metadata about a step, including custom names
     * and action/event IDs. Works for both populated and unpopulated funnels.
     */
    bool getFunnelStepMetadata(
        size_t step_index,
        char* custom_name_buffer,
        size_t name_buffer_size,
        char* action_id_buffer,
        size_t action_buffer_size
    );
    
    /**
     * @brief Compare step metrics across breakdowns
     * 
     * @param step_index Step to compare
     * @param counts Array to store counts per breakdown
     * @param conversion_rates Array to store conversion rates per breakdown
     * @return true if comparison data was retrieved
     * 
     * Retrieves counts and conversion rates for a specific step
     * across all breakdowns for comparison.
     */
    bool getFunnelBreakdownComparison(
        size_t step_index,
        uint32_t* counts,
        double* conversion_rates
    );
    
    /**
     * @brief Get funnel analysis time window
     * 
     * @param window_days Pointer to store window size in days
     * @return true if window configuration was found
     * 
     * Retrieves the configured time window for funnel analysis.
     * Converts weeks/months to approximate days if necessary.
     */
    bool getFunnelTimeWindow(uint32_t* window_days);

private:
    // Helper methods for insight type detection
    bool hasNumericCardStructure() const;
    bool hasLineGraphStructure() const;
    bool hasAreaChartStructure() const;

    DynamicJsonDocument doc;  ///< JSON document for parsing
    bool valid;              ///< Parsing status flag
}; 