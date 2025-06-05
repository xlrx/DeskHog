#pragma once

// It's good practice to place ArduinoJson config flags before including the library
// Ensure ARDUINOJSON_USE_PSRAM or ARDUINOJSON_ENABLE_PSRAM is defined in build_flags
// e.g., in platformio.ini: build_flags = -DARDUINOJSON_USE_PSRAM
#define ARDUINOJSON_DEFAULT_NESTING_LIMIT 50
#include <ArduinoJson.h>

// REMOVED: #define MAX_BREAKDOWNS 5 // This constant is likely defined elsewhere (e.g., InsightCard.h) using static constexpr

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
 * - Memory-efficient JSON parsing using ArduinoJson (leveraging PSRAM if enabled)
 * - Centralized data access for robustness against minor JSON structure variations
 * - Automatic insight type detection
 * - Comprehensive funnel analysis
 */
class InsightParser {
public:
    /**
     * @enum InsightType
     * @brief Supported visualization types for insights
     */
    enum class InsightType {
        NUMERIC_CARD,         ///< Single value display (e.g. total users)
        LINE_GRAPH,           ///< Time series line graph with date-based X-axis
        AREA_CHART,           ///< Area chart visualization with comparison data
        FUNNEL,               ///< Funnel visualization with steps and conversion metrics
        INSIGHT_NOT_SUPPORTED ///< Unsupported or unrecognized insight type
    };

    /**
     * @brief Constructor - parses JSON data
     * @param json Raw JSON string to parse
     * 
     * Initializes parser with JSON data and attempts to allocate memory.
     * On ESP32 platforms, will attempt to use PSRAM if available (requires ARDUINOJSON_USE_PSRAM build flag).
     * Uses isValid() to check if parsing was successful.
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
    bool getName(char* buffer, size_t bufferSize) const;
    
    /**
     * @brief Get numeric value for single-value insights
     * @return The numeric value or 0.0 if not applicable/available
     * 
     * Returns the aggregated_value from the first result for numeric card insights.
     * Should only be called if getInsightType() returns NUMERIC_CARD.
     */
    double getNumericCardValue() const;
    
    /**
     * @brief Get formatting prefix for numeric card value (e.g., "$")
     * @param buffer Buffer to store prefix
     * @param bufferSize Size of buffer
     * @return true if prefix was found and retrieved
     */
    bool getNumericFormattingPrefix(char* buffer, size_t bufferSize) const;

    /**
     * @brief Get formatting suffix for numeric card value (e.g., "%")
     * @param buffer Buffer to store suffix
     * @param bufferSize Size of buffer
     * @return true if suffix was found and retrieved
     */
    bool getNumericFormattingSuffix(char* buffer, size_t bufferSize) const;
    
    /**
     * @brief Get number of data points in line graph series
     * @return Number of points or 0 if not a line graph
     * 
     * Returns the number of time series data points for line graphs.
     * Should only be called if getInsightType() returns LINE_GRAPH.
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
    InsightType getInsightType() const;
    
    // Funnel-specific public methods
    
    /**
     * @brief Get number of funnel breakdowns
     * @return Number of breakdowns or 1 if no breakdowns
     * 
     * Returns the number of breakdown series in the funnel.
     * Limited by MAX_BREAKDOWNS (5), assuming MAX_BREAKDOWNS is defined globally/in a common header.
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
    ) const;
    
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
    ) const;
    
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
    ) const;
    
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
    ) const;
    
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
    ) const;
    
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
    ) const;
    
    /**
     * @brief Get funnel analysis time window
     * 
     * @param window_days Pointer to store window size in days
     * @return true if window configuration was found
     * 
     * Retrieves the configured time window for funnel analysis.
     * Converts weeks/months to approximate days if necessary.
     */
    bool getFunnelTimeWindow(uint32_t* window_days) const;

private:
    DynamicJsonDocument doc;              ///< JSON document for parsing (allocated on heap/PSRAM)
    bool valid;                         ///< Parsing status flag
    JsonObjectConst m_insightDataRoot;  ///< Points to the JsonObject containing the main "results" array

    // Private helper methods for insight type detection
    bool private_hasNumericCardStructure() const;
    bool private_hasLineGraphStructure() const;
    bool private_hasAreaChartStructure() const;
    bool private_hasFunnelStructure() const;
    bool private_hasFunnelResultData() const;
    bool private_hasFunnelNestedStructure() const;

    // Helper function to extract formatting string (prefix or suffix)
    static bool getFormattingString(const JsonObjectConst& query, const char* settingType, char* buffer, size_t bufferSize);
};

// Define common JSON keys as constants for readability and maintainability
// These should ideally be defined in a .cpp file if they are only used internally,
// but for ease of use across the parser, placing them in the header as static const char* is acceptable.
static const char* JSON_KEY_RESULTS = "results";
static const char* JSON_KEY_NAME = "name";
static const char* JSON_KEY_RESULT = "result";
static const char* JSON_KEY_QUERY = "query";
static const char* JSON_KEY_FILTERS = "filters";
static const char* JSON_KEY_INSIGHT = "insight"; // <--- ADDED THIS LINE
static const char* JSON_KEY_COMPARE = "compare";
static const char* JSON_KEY_DISPLAY = "display";
static const char* JSON_KEY_CHART_SETTINGS = "chartSettings";
static const char* JSON_KEY_TABLE_SETTINGS = "tableSettings";
static const char* JSON_KEY_YAXIS = "yAxis";
static const char* JSON_KEY_COLUMNS = "columns";
static const char* JSON_KEY_SETTINGS = "settings";
static const char* JSON_KEY_FORMATTING = "formatting";
static const char* JSON_KEY_PREFIX = "prefix";
static const char* JSON_KEY_SUFFIX = "suffix";
static const char* JSON_KEY_AGGREGATED_VALUE = "aggregated_value";
static const char* JSON_KEY_ORDER = "order";
static const char* JSON_KEY_COUNT = "count";
static const char* JSON_KEY_CUSTOM_NAME = "custom_name";
static const char* JSON_KEY_BREAKDOWN = "breakdown";
static const char* JSON_KEY_BREAKDOWN_VALUE = "breakdown_value";
static const char* JSON_KEY_AVERAGE_CONVERSION_TIME = "average_conversion_time";
static const char* JSON_KEY_MEDIAN_CONVERSION_TIME = "median_conversion_time";
static const char* JSON_KEY_FUNNEL_WINDOW_INTERVAL = "funnel_window_interval";
static const char* JSON_KEY_FUNNEL_WINDOW_INTERVAL_UNIT = "funnel_window_interval_unit";
static const char* JSON_KEY_EVENTS = "events";
static const char* JSON_KEY_ACTIONS = "actions";
static const char* JSON_KEY_ID = "id";
static const char* JSON_KEY_ACTION_ID = "action_id"; 

// Define common JSON values as constants
static const char* JSON_VAL_INSIGHT_FUNNELS = "FUNNELS";
static const char* JSON_VAL_DISPLAY_BOLD_NUMBER = "BoldNumber";
static const char* JSON_VAL_DISPLAY_ACTIONS_LINE_GRAPH = "ActionsLineGraph";
static const char* JSON_VAL_DISPLAY_ACTIONS_AREA_GRAPH = "ActionsAreaGraph"; // Assumed display type for area charts
static const char* JSON_VAL_FUNNEL_UNIT_DAY = "day";
static const char* JSON_VAL_FUNNEL_UNIT_WEEK = "week";
static const char* JSON_VAL_FUNNEL_UNIT_MONTH = "month";
