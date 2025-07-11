#include "InsightParser.h"
#include <stdio.h>
#include <algorithm> // Add for std::min

#ifdef ARDUINO
#include <Arduino.h>
#endif

// Filter to dramatically reduce memory usage by filtering out unused fields
static StaticJsonDocument<256> createFilter() {
    StaticJsonDocument<256> filter;
    filter[JSON_KEY_RESULTS][0][JSON_KEY_NAME] = true;
    filter[JSON_KEY_RESULTS][0][JSON_KEY_RESULT] = true;
    filter[JSON_KEY_RESULTS][0][JSON_KEY_QUERY][JSON_KEY_DISPLAY] = true;
    filter[JSON_KEY_RESULTS][0][JSON_KEY_QUERY][JSON_KEY_CHART_SETTINGS] = true;
    filter[JSON_KEY_RESULTS][0][JSON_KEY_QUERY][JSON_KEY_TABLE_SETTINGS] = true;
    filter[JSON_KEY_RESULTS][0][JSON_KEY_FILTERS][JSON_KEY_INSIGHT] = true; // <--- FIX: Used JSON_KEY_INSIGHT
    filter[JSON_KEY_RESULTS][0][JSON_KEY_FILTERS][JSON_KEY_EVENTS] = true;
    filter[JSON_KEY_RESULTS][0][JSON_KEY_FILTERS][JSON_KEY_ACTIONS] = true;
    filter[JSON_KEY_RESULTS][0][JSON_KEY_FILTERS][JSON_KEY_FUNNEL_WINDOW_INTERVAL] = true;
    filter[JSON_KEY_RESULTS][0][JSON_KEY_FILTERS][JSON_KEY_FUNNEL_WINDOW_INTERVAL_UNIT] = true;
    filter[JSON_KEY_RESULTS][0][JSON_KEY_COMPARE] = true; // Filter for "compare" at the results[0] level
    return filter;
}

InsightParser::InsightParser(const char* json) : doc(65536), valid(false) { // DynamicJsonDocument will allocate 64KB
    static StaticJsonDocument<256> filter = createFilter(); // Static filter for efficiency

#ifdef ARDUINO
    if (psramFound()) {
        size_t psramSize = ESP.getPsramSize();
        size_t psramFree = ESP.getFreePsram();
        Serial.printf("PSRAM available: %zu bytes, free: %zu bytes\n", psramSize, psramFree);

        if (psramFree < 65536) {
            Serial.println("Warning: Less than 64KB PSRAM free, parsing may fail");
        }
    } else {
        Serial.println("Warning: PSRAM not found, using SRAM for JSON parsing");
    }
#endif

    DeserializationError error = deserializeJson(doc, json, DeserializationOption::Filter(filter));
    if (error) {
        printf("JSON Deserialization failed: %s\n", error.c_str());
        return;
    }

    // --- Centralized m_insightDataRoot initialization and initial validation ---
    m_insightDataRoot = doc.as<JsonObjectConst>(); // Assuming the main insight object is at the root

    // Basic validation: ensure it's an object and contains a "results" key
    if (m_insightDataRoot.isNull() || !m_insightDataRoot.containsKey(JSON_KEY_RESULTS)) {
        printf("Insight JSON root is not an object or lacks the '%s' key.\n", JSON_KEY_RESULTS);
        return;
    }

    JsonArrayConst resultsArray = m_insightDataRoot[JSON_KEY_RESULTS];
    if (resultsArray.isNull() || resultsArray.size() == 0) {
        printf("'%s' array is null or empty.\n", JSON_KEY_RESULTS);
        return;
    }

    // Validate the first element of 'results' to ensure it's a typical insight object.
    // This is a "signature" check based on common, essential fields expected in an insight.
    JsonObjectConst firstInsightObject = resultsArray[0];
    if (firstInsightObject.isNull() ||
        // REMOVED: !firstInsightObject.is<JsonObjectConst>() || // <--- FIX: Redundant and incorrect syntax for JsonObjectConst
        !firstInsightObject.containsKey(JSON_KEY_NAME) ||
        !firstInsightObject.containsKey(JSON_KEY_RESULT) ||
        !firstInsightObject.containsKey(JSON_KEY_QUERY))
    {
        printf("First item in '%s' array lacks expected insight signature (e.g., %s, %s, or %s).\n",
               JSON_KEY_RESULTS, JSON_KEY_NAME, JSON_KEY_RESULT, JSON_KEY_QUERY);
        return;
    }
    // --- End m_insightDataRoot initialization and validation ---

    valid = true; // If we reached here, parsing and initial structure validation passed.
}

bool InsightParser::getName(char* buffer, size_t bufferSize) const {
    if (!valid || bufferSize == 0) {
        return false;
    }

    // Use m_insightDataRoot
    const char* name = m_insightDataRoot[JSON_KEY_RESULTS][0][JSON_KEY_NAME];
    if (!name) {
        return false;
    }

    strncpy(buffer, name, bufferSize - 1);
    buffer[bufferSize - 1] = '\0';
    return true;
}

double InsightParser::getNumericCardValue() const {
    if (!valid) {
        return 0.0;
    }

    // Use m_insightDataRoot
    JsonArrayConst results = m_insightDataRoot[JSON_KEY_RESULTS];
    if (results.isNull() || results.size() == 0) return 0.0;

    JsonObjectConst firstResultItem = results[0];
    if (firstResultItem.isNull()) return 0.0;

    JsonVariantConst resultField = firstResultItem[JSON_KEY_RESULT];
    if (resultField.isNull() || !resultField.is<JsonArrayConst>()) return 0.0;

    JsonArrayConst resultArray = resultField.as<JsonArrayConst>();
    if (resultArray.size() == 0) return 0.0;

    JsonVariantConst firstElementOfResultArray = resultArray[0];
    if (firstElementOfResultArray.isNull()) return 0.0;

    // Try old structure: results[0].result[0].aggregated_value
    if (firstElementOfResultArray.is<JsonObjectConst>()) {
        JsonObjectConst resultObject = firstElementOfResultArray.as<JsonObjectConst>();
        if (resultObject.containsKey(JSON_KEY_AGGREGATED_VALUE) && resultObject[JSON_KEY_AGGREGATED_VALUE].is<double>()) {
            return resultObject[JSON_KEY_AGGREGATED_VALUE].as<double>();
        }
    }

    // Try new structure: results[0].result[0][0]
    if (firstElementOfResultArray.is<JsonArrayConst>()) {
        JsonArrayConst innerArray = firstElementOfResultArray.as<JsonArrayConst>();
        if (innerArray.size() > 0 && innerArray[0].is<double>()) {
            return innerArray[0].as<double>();
        }
    }

    return 0.0; // Default if neither structure matches or value is not a double
}

bool InsightParser::isValid() const {
    return valid;
}

// Renamed and made private. All accessors must now use m_insightDataRoot
bool InsightParser::private_hasNumericCardStructure() const {
    if (!valid) return false;

    // Use m_insightDataRoot
    JsonArrayConst results = m_insightDataRoot[JSON_KEY_RESULTS];
    if (results.isNull() || results.size() == 0) return false;

    JsonObjectConst firstResultItem = results[0];
    if (firstResultItem.isNull()) return false;

    JsonVariantConst resultField = firstResultItem[JSON_KEY_RESULT];
    if (resultField.isNull() || !resultField.is<JsonArrayConst>()) return false;

    JsonArrayConst resultArray = resultField.as<JsonArrayConst>();
    if (resultArray.size() == 0) return false;

    JsonVariantConst firstElementOfResultArray = resultArray[0];
    if (firstElementOfResultArray.isNull()) return false;

    // Old structure: results[0].result[0].aggregated_value
    if (firstElementOfResultArray.is<JsonObjectConst>()) {
        JsonObjectConst resultObject = firstElementOfResultArray.as<JsonObjectConst>();
        return resultObject.containsKey(JSON_KEY_AGGREGATED_VALUE) && resultObject[JSON_KEY_AGGREGATED_VALUE].is<double>();
    }

    // New structure: results[0].result[0][0]
    if (firstElementOfResultArray.is<JsonArrayConst>()) {
        JsonArrayConst innerArray = firstElementOfResultArray.as<JsonArrayConst>();
        if (innerArray.size() > 0 && innerArray[0].is<double>()) {
            return true;
        }
    }
    
    // Also check for "BoldNumber" display type if present, as an explicit hint
    const char* displayType = firstResultItem[JSON_KEY_QUERY][JSON_KEY_DISPLAY];
    if (displayType && strcmp(displayType, JSON_VAL_DISPLAY_BOLD_NUMBER) == 0) {
        // If display is BoldNumber, it's highly likely a numeric card, even if result structure is minimal
        return true;
    }

    return false;
}

// Renamed and made private. All accessors must now use m_insightDataRoot
bool InsightParser::private_hasLineGraphStructure() const {
    if (!valid) return false;

    // Use m_insightDataRoot
    JsonArrayConst results = m_insightDataRoot[JSON_KEY_RESULTS];
    if (results.isNull() || results.size() == 0) return false;

    JsonObjectConst firstResult = results[0];
    if (firstResult.isNull()) return false;
    
    // Check for line graph structure:
    // - results array exists
    // - first result has result array with multiple points
    JsonArrayConst timeseriesData = firstResult[JSON_KEY_RESULT];
    if (timeseriesData.isNull() || timeseriesData.size() <= 1) return false; // Needs at least 2 points for a line graph

    // Additional check: verify it's explicitly a line graph if display type is present
    const char* displayType = firstResult[JSON_KEY_QUERY][JSON_KEY_DISPLAY];
    if (displayType && strcmp(displayType, JSON_VAL_DISPLAY_ACTIONS_LINE_GRAPH) == 0) {
        return true;
    }
    
    // Check first data point has expected time series structure [date_string, numeric_value]
    JsonArrayConst firstPoint = timeseriesData[0];
    if (firstPoint.isNull() || firstPoint.size() != 2) return false;
    
    // Time series first element should be a date string
    const char* dateStr = firstPoint[0];
    if (!dateStr || strlen(dateStr) < 10) return false;  // YYYY-MM-DD minimum
    
    // Second element should be a numeric value
    return firstPoint[1].is<double>();
}

// Renamed and made private. All accessors must now use m_insightDataRoot
bool InsightParser::private_hasAreaChartStructure() const {
    if (!valid) return false;
    
    // Area charts are similar to line graphs but typically have
    // an additional "compare" property or explicit display type.

    // Use m_insightDataRoot
    JsonArrayConst results = m_insightDataRoot[JSON_KEY_RESULTS];
    if (results.isNull() || results.size() == 0) return false;
    
    JsonObjectConst firstResult = results[0];
    if (firstResult.isNull()) return false;

    // Primary check: explicit display type
    const char* displayType = firstResult[JSON_KEY_QUERY][JSON_KEY_DISPLAY];
    if (displayType && strcmp(displayType, JSON_VAL_DISPLAY_ACTIONS_AREA_GRAPH) == 0) {
        // If the display type is explicitly AreaGraph, ensure it also has line graph data structure
        return private_hasLineGraphStructure(); 
    }

    // Secondary check: presence of "compare" flag AND line graph structure.
    // Note: The `compare` flag can also be in `filters`. Check both for robustness.
    bool hasCompareFlag = !firstResult[JSON_KEY_COMPARE].isNull(); // Directly in insight object
    if (!hasCompareFlag) {
        JsonObjectConst filters = firstResult[JSON_KEY_FILTERS];
        if (!filters.isNull()) {
            hasCompareFlag = filters.containsKey(JSON_KEY_COMPARE); // Check if key exists
        }
    }

    if (hasCompareFlag) {
        return private_hasLineGraphStructure(); // If compare is present and it looks like a line graph, treat as area
    }

    return false;
}

// Renamed from detectInsightType, added const
InsightParser::InsightType InsightParser::getInsightType() const {
    if (!valid) return InsightType::INSIGHT_NOT_SUPPORTED;
    
    // Order of checks: from most specific/unique identifier to more general.
    // Funnel is often uniquely identified by filters.insight="FUNNELS"
    if (private_hasFunnelStructure()) return InsightType::FUNNEL;
    
    // Numeric card has a distinct result structure or "BoldNumber" display type
    if (private_hasNumericCardStructure()) return InsightType::NUMERIC_CARD;

    // Area charts are a specific type of line graph, often with "compare" data or explicit display.
    // Check this before generic line graph.
    if (private_hasAreaChartStructure()) return InsightType::AREA_CHART;
    
    // Line graphs are more generic time series.
    if (private_hasLineGraphStructure()) return InsightType::LINE_GRAPH;
    
    return InsightType::INSIGHT_NOT_SUPPORTED;
}

// Renamed and made private. All accessors must now use m_insightDataRoot
bool InsightParser::private_hasFunnelStructure() const {
    if (!valid) return false;
    
    // Use m_insightDataRoot
    JsonObjectConst firstResult = m_insightDataRoot[JSON_KEY_RESULTS][0];
    if (firstResult.isNull()) return false; // Should be handled by constructor validation, but defensiveness

    JsonObjectConst filters = firstResult[JSON_KEY_FILTERS];
    if (filters.isNull()) return false;

    // Check if the insight type is explicitly set to FUNNELS
    const char* insightType = filters[JSON_KEY_INSIGHT]; // <--- FIX: Used JSON_KEY_INSIGHT
    if (insightType && strcmp(insightType, JSON_VAL_INSIGHT_FUNNELS) == 0) {
        return true;
    }
    
    return false;
}

// Renamed and made private. All accessors must now use m_insightDataRoot
bool InsightParser::private_hasFunnelResultData() const {
    if (!valid || !private_hasFunnelStructure()) return false;
    
    // Use m_insightDataRoot
    JsonVariantConst result = m_insightDataRoot[JSON_KEY_RESULTS][0][JSON_KEY_RESULT];
    if (result.isNull() || result.size() == 0) return false;
    
    // Try to access the first element
    JsonVariantConst firstElement = result[0];
    if (firstElement.isNull()) return false;
    
    // For non-nested (flat) structure, check if the first element has expected properties (count, order)
    if (firstElement.is<JsonObjectConst>()) {
        JsonObjectConst firstObject = firstElement.as<JsonObjectConst>();
        if (firstObject.containsKey(JSON_KEY_COUNT) && firstObject.containsKey(JSON_KEY_ORDER)) {
            return true;
        }
    }

    // For nested structure, check the first item as an array, then its first element
    if (firstElement.is<JsonArrayConst>()) {
        JsonArrayConst firstBreakdown = firstElement.as<JsonArrayConst>(); // Ensure it's treated as array
        if (firstBreakdown.isNull() || firstBreakdown.size() == 0) return false;
        
        JsonObjectConst firstStep = firstBreakdown[0];
        // Ensure first step is an object and has count/order
        return !firstStep.isNull() && !firstStep[JSON_KEY_ORDER].isNull() && !firstStep[JSON_KEY_COUNT].isNull();
    }
    
    return false;
}

// Renamed and made private. All accessors must now use m_insightDataRoot
bool InsightParser::private_hasFunnelNestedStructure() const {
    if (!valid || !private_hasFunnelStructure() || !private_hasFunnelResultData()) return false;
    
    // Use m_insightDataRoot
    JsonVariantConst result = m_insightDataRoot[JSON_KEY_RESULTS][0][JSON_KEY_RESULT];
    JsonVariantConst firstElement = result[0];
    
    // If first element is an array, it's a nested structure (e.g., [[step1], [step2]])
    if (firstElement.is<JsonArrayConst>()) {
        return true;
    }
    
    return false; // For flat structure, result[0] is typically an object directly.
}

size_t InsightParser::getSeriesPointCount() const {
    if (!valid || !private_hasLineGraphStructure()) return 0;
    
    // Use m_insightDataRoot
    JsonArrayConst results = m_insightDataRoot[JSON_KEY_RESULTS];
    JsonArrayConst timeseriesData = results[0][JSON_KEY_RESULT];
    return timeseriesData.size();
}

bool InsightParser::getSeriesYValues(double* yValues) const {
    if (!valid || !private_hasLineGraphStructure() || !yValues) return false;
    
    // Use m_insightDataRoot
    JsonArrayConst results = m_insightDataRoot[JSON_KEY_RESULTS];
    JsonArrayConst timeseriesData = results[0][JSON_KEY_RESULT];
    size_t pointCount = timeseriesData.size();
    
    // Extract y-values directly - format is consistent with [date_string, numeric_value]
    for (size_t i = 0; i < pointCount; i++) {
        yValues[i] = timeseriesData[i][1].as<double>();
    }
    
    return true;
}

bool InsightParser::getSeriesXLabel(size_t index, char* buffer, size_t bufferSize) const {
    if (!valid || !private_hasLineGraphStructure() || !buffer || bufferSize == 0) return false;
    
    // Use m_insightDataRoot
    JsonArrayConst results = m_insightDataRoot[JSON_KEY_RESULTS];
    JsonArrayConst timeseriesData = results[0][JSON_KEY_RESULT];
    
    if (index >= timeseriesData.size()) return false;
    
    const char* dateStr = timeseriesData[index][0];
    if (!dateStr) return false;
    
    // Copy just the year and month (YYYY-MM) to keep labels compact
    size_t len = strlen(dateStr);
    if (len < 7) return false;
    
    size_t copyLen = std::min((size_t)7, bufferSize - 1);
    strncpy(buffer, dateStr, copyLen);
    buffer[copyLen] = '\0'; // Always null-terminate
    
    return true;
}

void InsightParser::getSeriesRange(double* minValue, double* maxValue) const {
    if (!valid || !private_hasLineGraphStructure() || !minValue || !maxValue) {
        if (minValue) *minValue = 0.0;
        if (maxValue) *maxValue = 0.0;
        return;
    }
    
    // Use m_insightDataRoot
    JsonArrayConst results = m_insightDataRoot[JSON_KEY_RESULTS];
    JsonArrayConst timeseriesData = results[0][JSON_KEY_RESULT];
    
    if (timeseriesData.size() == 0) {
        *minValue = 0.0;
        *maxValue = 0.0;
        return;
    }

    *minValue = timeseriesData[0][1].as<double>();
    *maxValue = *minValue;
    
    for (size_t i = 1; i < timeseriesData.size(); i++) {
        double value = timeseriesData[i][1].as<double>();
        if (value < *minValue) *minValue = value;
        if (value > *maxValue) *maxValue = value;
    }
}

size_t InsightParser::getFunnelBreakdownCount() const {
    if (!valid || !private_hasFunnelStructure()) return 0;
    
    // Check if we have a nested or flat structure
    bool isNested = private_hasFunnelNestedStructure();
    
    if (isNested) {
        // Use m_insightDataRoot
        JsonArrayConst result = m_insightDataRoot[JSON_KEY_RESULTS][0][JSON_KEY_RESULT];
        
        // Ensure the result is an array
        if (result.isNull()) {
            return 0;
        }
        
        // Use literal 5 instead of MAX_BREAKDOWNS macro
        return std::min(result.size(), (size_t)5);
    } else {
        // For flat structure, we always have exactly one breakdown ("All users")
        return 1;
    }
}

size_t InsightParser::getFunnelStepCount() const {
    if (!valid || !private_hasFunnelStructure()) return 0;
    
    // For unpopulated funnels, count events and actions from filters
    if (!private_hasFunnelResultData()) {
        size_t count = 0;
        
        // Use m_insightDataRoot
        JsonObjectConst filters = m_insightDataRoot[JSON_KEY_RESULTS][0][JSON_KEY_FILTERS];
        if (filters.isNull()) return 0;

        JsonArrayConst events = filters[JSON_KEY_EVENTS];
        if (!events.isNull()) {
            count += events.size();
        }
        
        JsonArrayConst actions = filters[JSON_KEY_ACTIONS];
        if (!actions.isNull()) {
            count += actions.size();
        }
        
        return count;
    }
    
    // Use m_insightDataRoot
    JsonArrayConst result = m_insightDataRoot[JSON_KEY_RESULTS][0][JSON_KEY_RESULT];
    if (result.isNull()) return 0;
    
    // For flat structure, count the items in the result array
    if (!private_hasFunnelNestedStructure()) {
        return result.size();
    }
    
    // For nested structure, count steps in the first breakdown
    JsonArrayConst firstBreakdown = result[0];
    return firstBreakdown.size();
}

bool InsightParser::getFunnelStepData(
    size_t breakdown_index,
    size_t step_index,
    char* name_buffer,
    size_t name_buffer_size,
    uint32_t* count,
    double* conversion_time_avg,
    double* conversion_time_median
) const {
    if (!valid || !private_hasFunnelStructure()) return false;
    
    // Handle unpopulated funnels
    if (!private_hasFunnelResultData()) {
        if (breakdown_index > 0) return false;
        
        // Use m_insightDataRoot
        JsonObjectConst filters = m_insightDataRoot[JSON_KEY_RESULTS][0][JSON_KEY_FILTERS];
        if (filters.isNull()) return false;

        // Get combined list of events and actions
        JsonArrayConst events = filters[JSON_KEY_EVENTS];
        JsonArrayConst actions = filters[JSON_KEY_ACTIONS];
        
        size_t total_steps = 0;
        if (!events.isNull()) total_steps += events.size();
        if (!actions.isNull()) total_steps += actions.size();
        
        if (step_index >= total_steps) return false;
        
        // Determine if we need to get from events or actions
        JsonObjectConst step;
        if (!events.isNull() && step_index < events.size()) {
            step = events[step_index];
        } else if (!actions.isNull()) {
            step = actions[step_index - (events.isNull() ? 0 : events.size())];
        } else {
            return false;
        }
        
        // Get step name if buffer provided
        if (name_buffer && name_buffer_size > 0) {
            const char* custom_name = step[JSON_KEY_CUSTOM_NAME];
            if (custom_name) {
                strncpy(name_buffer, custom_name, name_buffer_size - 1);
                name_buffer[name_buffer_size - 1] = '\0';
            } else {
                const char* name = step[JSON_KEY_NAME];
                if (name) {
                    strncpy(name_buffer, name, name_buffer_size - 1);
                    name_buffer[name_buffer_size - 1] = '\0';
                } else {
                    name_buffer[0] = '\0';
                }
            }
        }
        
        // For null result, we don't have count and conversion times
        if (count) *count = 0;
        if (conversion_time_avg) *conversion_time_avg = 0.0;
        if (conversion_time_median) *conversion_time_median = 0.0;
        
        return true;
    }
    
    // Use m_insightDataRoot
    JsonArrayConst result = m_insightDataRoot[JSON_KEY_RESULTS][0][JSON_KEY_RESULT];
    if (result.isNull()) return false;
    
    JsonObjectConst step;
    
    // Handle flat structure
    if (!private_hasFunnelNestedStructure()) {
        // For flat structure, ignore breakdown_index (except 0)
        if (breakdown_index > 0) return false;
        
        // Check step_index is valid
        if (step_index >= result.size()) return false;
        
        // Get step directly from result
        step = result[step_index];
    } else {
        // Handle nested structure
        // Check breakdown_index is valid
        if (breakdown_index >= result.size() || breakdown_index >= 5) return false; // Use literal 5

        JsonArrayConst breakdown = result[breakdown_index];
        if (breakdown.isNull()) return false;
        
        // Check step_index is valid
        if (step_index >= breakdown.size()) return false;
        
        // Get step from appropriate breakdown
        step = breakdown[step_index];
    }

    if (step.isNull()) return false;
    
    // Extract data from step (same for both structures)
    // Get step name if buffer provided
    if (name_buffer && name_buffer_size > 0) {
        const char* custom_name = step[JSON_KEY_CUSTOM_NAME];
        if (custom_name) {
            strncpy(name_buffer, custom_name, name_buffer_size - 1);
            name_buffer[name_buffer_size - 1] = '\0';
        } else {
            const char* name = step[JSON_KEY_NAME];
            if (name) {
                strncpy(name_buffer, name, name_buffer_size - 1);
                name_buffer[name_buffer_size - 1] = '\0';
            } else {
                name_buffer[0] = '\0';
            }
        }
    }
    
    // Get count
    if (count) {
        *count = step[JSON_KEY_COUNT].as<uint32_t>();
    }
    
    // Get conversion times
    if (conversion_time_avg) {
        *conversion_time_avg = step[JSON_KEY_AVERAGE_CONVERSION_TIME].as<double>();
    }
    
    if (conversion_time_median) {
        *conversion_time_median = step[JSON_KEY_MEDIAN_CONVERSION_TIME].as<double>();
    }
    
    return true;
}

bool InsightParser::getFunnelBreakdownName(
    size_t breakdown_index,
    char* name_buffer,
    size_t buffer_size
) const {
    if (!valid || !private_hasFunnelStructure() || !name_buffer || buffer_size == 0) return false;

    // Check if we have a nested or flat structure
    bool isNested = private_hasFunnelNestedStructure();

    if (isNested) {
        // Use m_insightDataRoot
        JsonArrayConst result = m_insightDataRoot[JSON_KEY_RESULTS][0][JSON_KEY_RESULT];

        // Ensure the result is an array
        if (result.isNull()) {
            return false;
        }

        if (breakdown_index >= result.size() || breakdown_index >= 5) { // Use literal 5
            return false;
        }

        // Get the breakdown name from the first step's breakdown field
        JsonArrayConst breakdown_array = result[breakdown_index];
        if (!breakdown_array.isNull() && breakdown_array.size() > 0) {
            JsonObjectConst firstStepInBreakdown = breakdown_array[0];
            if (!firstStepInBreakdown.isNull() && firstStepInBreakdown.containsKey(JSON_KEY_BREAKDOWN)) {
                JsonArrayConst breakdown_data = firstStepInBreakdown[JSON_KEY_BREAKDOWN];
                if (!breakdown_data.isNull() && breakdown_data.size() > 0) {
                    const char* value = breakdown_data[0].as<const char*>();
                    if (value) {
                        strncpy(name_buffer, value, buffer_size - 1);
                        name_buffer[buffer_size - 1] = '\0';
                        return true;
                    }
                }
            }
        }
    } else {
        // For flat structure, we only have one breakdown
        if (breakdown_index > 0) {
            return false;
        }
        
        // In flat structure, default to "All users"
        strncpy(name_buffer, "All users", buffer_size - 1);
        name_buffer[buffer_size - 1] = '\0';
        return true;
    }

    name_buffer[0] = '\0';
    return false;
}

bool InsightParser::getFunnelTotalCounts(
    size_t breakdown_index,
    uint32_t* counts,
    double* conversion_rates
) const {
    if (!valid || !private_hasFunnelStructure() || !counts) return false;
    
    // Check if this is a flat or nested structure
    bool isNested = private_hasFunnelNestedStructure();
    size_t stepCount = getFunnelStepCount();
    
    if (stepCount == 0) {
        return false;
    }
    
    // Clear the buffer first
    for (size_t i = 0; i < stepCount; i++) {
        counts[i] = 0;
    }

    // Use m_insightDataRoot
    JsonArrayConst result = m_insightDataRoot[JSON_KEY_RESULTS][0][JSON_KEY_RESULT];
    if (result.isNull()) {
        return false;
    }
    
    // For nested structure, we sum counts across all breakdowns for each step
    if (isNested) {
        size_t breakdownCount = std::min(result.size(), (size_t)5); // Use literal 5
        for (size_t bd_idx = 0; bd_idx < breakdownCount; bd_idx++) {
            JsonArrayConst breakdown_array = result[bd_idx];
            if (!breakdown_array.isNull()) {
                // For each step in this breakdown
                size_t stepsInBreakdown = std::min(breakdown_array.size(), stepCount);
                for (size_t step_idx = 0; step_idx < stepsInBreakdown; step_idx++) {
                    counts[step_idx] += breakdown_array[step_idx][JSON_KEY_COUNT].as<uint32_t>();
                }
            }
        }
    } else {
        // Simply copy from the flat structure directly (only one breakdown)
        for (size_t step_idx = 0; step_idx < stepCount; step_idx++) {
            counts[step_idx] = result[step_idx][JSON_KEY_COUNT].as<uint32_t>();
        }
    }
    
    // Calculate conversion rates if requested
    if (conversion_rates && counts[0] > 0) {
        for (size_t i = 0; i < stepCount; i++) {
            if (i == 0) {
                conversion_rates[i] = 1.0;
            } else if (counts[i-1] > 0) {
                conversion_rates[i] = (double)counts[i] / counts[i-1];
            } else {
                conversion_rates[i] = 0.0;
            }
        }
    } else if (conversion_rates) {
        for (size_t i = 0; i < stepCount; i++) {
            conversion_rates[i] = 0.0;
        }
    }
    
    return true;
}

bool InsightParser::getFunnelConversionTimes(
    size_t breakdown_index,
    size_t step_index,
    double* avg_time,
    double* median_time
) const {
    if (!valid || !private_hasFunnelStructure()) return false;
    if (step_index == 0) return false;
    
    // For unpopulated funnels, we can't provide conversion times
    if (!private_hasFunnelResultData()) return false;
    
    // Use m_insightDataRoot
    JsonArrayConst result = m_insightDataRoot[JSON_KEY_RESULTS][0][JSON_KEY_RESULT];
    if (result.isNull()) return false;

    // For flat structure
    if (!private_hasFunnelNestedStructure()) {
        // Only support breakdown_index 0 for flat structure
        if (breakdown_index > 0) return false;
        
        // Check step_index is valid
        if (step_index >= result.size()) return false;
        
        // Get conversion times directly from step
        JsonObjectConst step = result[step_index];
        if (step.isNull()) return false;

        if (avg_time) {
            *avg_time = step[JSON_KEY_AVERAGE_CONVERSION_TIME].as<double>();
        }
        if (median_time) {
            *median_time = step[JSON_KEY_MEDIAN_CONVERSION_TIME].as<double>();
        }
        
        return true;
    }
    
    // For nested structure, call getFunnelStepData which handles it
    return getFunnelStepData(
        breakdown_index,
        step_index,
        nullptr,
        0,
        nullptr,
        avg_time,
        median_time
    );
}

bool InsightParser::getFunnelStepMetadata(
    size_t step_index,
    char* custom_name_buffer,
    size_t name_buffer_size,
    char* action_id_buffer,
    size_t action_buffer_size
) const {
    if (!valid || !private_hasFunnelStructure()) return false;
    
    // For unpopulated funnels, get metadata from filters
    if (!private_hasFunnelResultData()) {
        // Use m_insightDataRoot
        JsonObjectConst filters = m_insightDataRoot[JSON_KEY_RESULTS][0][JSON_KEY_FILTERS];
        if (filters.isNull()) return false;

        JsonArrayConst events = filters[JSON_KEY_EVENTS];
        JsonArrayConst actions = filters[JSON_KEY_ACTIONS];
        
        size_t total_steps = 0;
        if (!events.isNull()) total_steps += events.size();
        if (!actions.isNull()) total_steps += actions.size();
        
        if (step_index >= total_steps) return false;
        
        // Determine if we need to get from events or actions
        JsonObjectConst step;
        if (!events.isNull() && step_index < events.size()) {
            step = events[step_index];
        } else if (!actions.isNull()) {
            step = actions[step_index - (events.isNull() ? 0 : events.size())];
        } else {
            return false;
        }
        
        // Get custom name
        if (custom_name_buffer && name_buffer_size > 0) {
            const char* custom_name = step[JSON_KEY_CUSTOM_NAME];
            if (custom_name) {
                strncpy(custom_name_buffer, custom_name, name_buffer_size - 1);
                custom_name_buffer[name_buffer_size - 1] = '\0';
            } else {
                custom_name_buffer[0] = '\0';
            }
        }
        
        // Get action ID (or event ID, typically "id" field in filters)
        if (action_id_buffer && action_buffer_size > 0) {
            const char* id = step[JSON_KEY_ID];
            if (id) {
                strncpy(action_id_buffer, id, action_buffer_size - 1);
                action_id_buffer[action_buffer_size - 1] = '\0';
            } else {
                action_id_buffer[0] = '\0';
            }
        }
        
        return true;
    }
    
    // For populated funnels
    // Use m_insightDataRoot
    JsonVariantConst result = m_insightDataRoot[JSON_KEY_RESULTS][0][JSON_KEY_RESULT];
    if (result.isNull()) return false;

    JsonObjectConst step;
    
    // For flat structure
    if (!private_hasFunnelNestedStructure()) {
        if (step_index >= result.size()) return false;
        step = result[step_index];
    } else {
        // For nested structure, use the first breakdown to get step metadata (names/IDs are consistent)
        JsonArrayConst firstBreakdown = result[0];
        if (firstBreakdown.isNull() || step_index >= firstBreakdown.size()) return false;
        step = firstBreakdown[step_index];
    }

    if (step.isNull()) return false;
    
    // Get custom name
    if (custom_name_buffer && name_buffer_size > 0) {
        const char* custom_name = step[JSON_KEY_CUSTOM_NAME];
        if (custom_name) {
            strncpy(custom_name_buffer, custom_name, name_buffer_size - 1);
            custom_name_buffer[name_buffer_size - 1] = '\0';
        } else {
            custom_name_buffer[0] = '\0';
        }
    }
    
    // Get action ID (for result data, it's typically "action_id")
    if (action_id_buffer && action_buffer_size > 0) {
        const char* action_id = step[JSON_KEY_ACTION_ID];
        if (action_id) {
            strncpy(action_id_buffer, action_id, action_buffer_size - 1);
            action_id_buffer[action_buffer_size - 1] = '\0';
        } else {
            action_id_buffer[0] = '\0';
        }
    }
    
    return true;
}

bool InsightParser::getFunnelBreakdownComparison(
    size_t step_index,
    uint32_t* counts,
    double* conversion_rates
) const {
    if (!valid || !private_hasFunnelStructure() || !counts) return false;

    // Check if this is a flat or nested structure
    bool isNested = private_hasFunnelNestedStructure();

    // Use m_insightDataRoot
    JsonArrayConst result = m_insightDataRoot[JSON_KEY_RESULTS][0][JSON_KEY_RESULT];
    if (result.isNull()) {
        return false;
    }

    size_t breakdownCount = getFunnelBreakdownCount();
    if (breakdownCount == 0) {
        return false;
    }

    // Initialize counts and conversion_rates. Use literal 5 for MAX_BREAKDOWNS limit.
    for (size_t i = 0; i < 5; i++) {
        counts[i] = 0;
        if (conversion_rates) conversion_rates[i] = 0.0;
    }

    if (isNested) {
        // For each breakdown, get the count for this step
        for (size_t bd_idx = 0; bd_idx < breakdownCount; bd_idx++) {
            JsonArrayConst breakdown_array = result[bd_idx];
            if (!breakdown_array.isNull() && step_index < breakdown_array.size()) {
                JsonObjectConst stepObj = breakdown_array[step_index];
                if (!stepObj.isNull()) {
                    // Extract count
                    counts[bd_idx] = stepObj[JSON_KEY_COUNT].as<uint32_t>();
                    
                    // Calculate conversion rate compared to first step of THIS breakdown
                    if (conversion_rates && breakdown_array.size() > 0) {
                        uint32_t firstStepCount = breakdown_array[0][JSON_KEY_COUNT].as<uint32_t>();
                        if (firstStepCount > 0) {
                            conversion_rates[bd_idx] = (double)counts[bd_idx] / firstStepCount;
                        } else {
                            conversion_rates[bd_idx] = 0.0;
                        }
                    }
                }
            }
        }
    } else {
        // Flat structure implementation (only one breakdown at index 0)
        if (step_index >= result.size()) {
            return false;
        }
        
        counts[0] = result[step_index][JSON_KEY_COUNT].as<uint32_t>();
        
        // Calculate conversion rate compared to first step of the flat funnel
        if (conversion_rates && result.size() > 0) {
            uint32_t firstStepCount = result[0][JSON_KEY_COUNT].as<uint32_t>();
            if (firstStepCount > 0) {
                conversion_rates[0] = (double)counts[0] / firstStepCount;
            } else {
                conversion_rates[0] = 0.0;
            }
        }
    }
    
    return true;
}

bool InsightParser::getFunnelTimeWindow(uint32_t* window_days) const {
    if (!valid || !private_hasFunnelStructure() || !window_days) return false;
    
    // Use m_insightDataRoot
    JsonObjectConst filters = m_insightDataRoot[JSON_KEY_RESULTS][0][JSON_KEY_FILTERS];
    if (filters.isNull()) return false;
    
    uint32_t interval = filters[JSON_KEY_FUNNEL_WINDOW_INTERVAL] | 0;
    if (interval == 0) return false;

    // Check interval unit
    const char* unit = filters[JSON_KEY_FUNNEL_WINDOW_INTERVAL_UNIT];
    if (unit) {
        if (strcmp(unit, JSON_VAL_FUNNEL_UNIT_DAY) == 0) {
            *window_days = interval;
            return true;
        } else if (strcmp(unit, JSON_VAL_FUNNEL_UNIT_WEEK) == 0) {
            *window_days = interval * 7;
            return true;
        } else if (strcmp(unit, JSON_VAL_FUNNEL_UNIT_MONTH) == 0) {
            *window_days = interval * 30;  // Approximate
            return true;
        }
    }
    
    // Default to days if no unit specified or unit is unrecognized
    *window_days = interval;
    return true;
}


// Helper function to extract formatting string (prefix or suffix)
bool InsightParser::getFormattingString(const JsonObjectConst& query, const char* settingType, char* buffer, size_t bufferSize) {
    if (query.isNull() || bufferSize == 0) {
        if (buffer) buffer[0] = '\0';
        return false;
    }
    if (buffer) buffer[0] = '\0';

    // Try chartSettings first
    if (query.containsKey(JSON_KEY_CHART_SETTINGS)) {
        JsonObjectConst chartSettings = query[JSON_KEY_CHART_SETTINGS];
        if (!chartSettings.isNull() && chartSettings.containsKey(JSON_KEY_YAXIS)) {
            JsonArrayConst yAxis = chartSettings[JSON_KEY_YAXIS];
            if (!yAxis.isNull() && yAxis.size() > 0) {
                JsonObjectConst yAxisSettings = yAxis[0];
                if (!yAxisSettings.isNull() && yAxisSettings.containsKey(JSON_KEY_SETTINGS)) {
                    JsonObjectConst settings = yAxisSettings[JSON_KEY_SETTINGS];
                    if (!settings.isNull() && settings.containsKey(JSON_KEY_FORMATTING)) {
                        JsonObjectConst formatting = settings[JSON_KEY_FORMATTING];
                        if (!formatting.isNull() && formatting.containsKey(settingType)) {
                            const char* value = formatting[settingType];
                            if (value) {
                                strncpy(buffer, value, bufferSize - 1);
                                buffer[bufferSize - 1] = '\0';
                                return true;
                            }
                        }
                    }
                }
            }
        }
    }

    // Fallback to tableSettings
    if (query.containsKey(JSON_KEY_TABLE_SETTINGS)) {
        JsonObjectConst tableSettings = query[JSON_KEY_TABLE_SETTINGS];
        if (!tableSettings.isNull() && tableSettings.containsKey(JSON_KEY_COLUMNS)) {
            JsonArrayConst columns = tableSettings[JSON_KEY_COLUMNS];
            if (!columns.isNull() && columns.size() > 0) {
                JsonObjectConst columnSettings = columns[0];
                if (!columnSettings.isNull() && columnSettings.containsKey(JSON_KEY_SETTINGS)) {
                    JsonObjectConst settings = columnSettings[JSON_KEY_SETTINGS];
                    if (!settings.isNull() && settings.containsKey(JSON_KEY_FORMATTING)) {
                        JsonObjectConst formatting = settings[JSON_KEY_FORMATTING];
                        if (!formatting.isNull() && formatting.containsKey(settingType)) {
                            const char* value = formatting[settingType];
                            if (value) {
                                strncpy(buffer, value, bufferSize - 1);
                                buffer[bufferSize - 1] = '\0';
                                return true;
                            }
                        }
                    }
                }
            }
        }
    }

    return false;
}

bool InsightParser::getNumericFormattingPrefix(char* buffer, size_t bufferSize) const {
    if (!valid || bufferSize == 0) {
        if (buffer) buffer[0] = '\0';
        return false;
    }
    // Use m_insightDataRoot
    JsonObjectConst query = m_insightDataRoot[JSON_KEY_RESULTS][0][JSON_KEY_QUERY];
    return getFormattingString(query, JSON_KEY_PREFIX, buffer, bufferSize);
}

bool InsightParser::getNumericFormattingSuffix(char* buffer, size_t bufferSize) const {
    if (!valid || bufferSize == 0) {
        if (buffer) buffer[0] = '\0';
        return false;
    }
    // Use m_insightDataRoot
    JsonObjectConst query = m_insightDataRoot[JSON_KEY_RESULTS][0][JSON_KEY_QUERY];
    return getFormattingString(query, JSON_KEY_SUFFIX, buffer, bufferSize);
}