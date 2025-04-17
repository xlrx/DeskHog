#include "InsightParser.h"
#include <stdio.h>
#include <algorithm> // Add for std::min

#ifdef ARDUINO
#include <Arduino.h>
#endif

// Define constant to match the one in InsightCard.h
#define MAX_BREAKDOWNS 5

// Filter to dramatically reduce memory usage by filtering out unused fields
static StaticJsonDocument<128> createFilter() {
    StaticJsonDocument<128> filter;
    filter["results"][0]["name"] = true;
    filter["results"][0]["result"] = true;
    filter["results"][0]["query"]["display"] = true;
    filter["results"][0]["filters"]["insight"] = true;
    filter["results"][0]["compare"] = true;
    return filter;
}

InsightParser::InsightParser(const char* json) : doc(65536), valid(false) {  // Reduced size from 256KB to 64KB
    static StaticJsonDocument<128> filter = createFilter();
    
    // Initialize PSRAM if available
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
    JsonArrayConst results = doc["results"];
    if (results.isNull() || results.size() == 0) return false;
    
    JsonArrayConst timeseriesData = results[0]["result"];
    if (timeseriesData.isNull() || timeseriesData.size() <= 1) return false;
    
    // Additional check: verify it's explicitly a line graph if display type is present
    const char* displayType = results[0]["query"]["display"];
    if (displayType && strcmp(displayType, "ActionsLineGraph") == 0) {
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
    if (hasFunnelStructure()) return InsightType::FUNNEL;
    
    return InsightType::INSIGHT_NOT_SUPPORTED;
}

bool InsightParser::hasFunnelStructure() const {
    if (!valid) return false;
    
    // Check if the insight type is explicitly set to FUNNELS
    const char* insightType = doc["results"][0]["filters"]["insight"];
    if (insightType && strcmp(insightType, "FUNNELS") == 0) {
        return true;
    }
    
    return false;
}

bool InsightParser::hasFunnelResultData() const {
    if (!valid || !hasFunnelStructure()) return false;
    
    // Check if result is populated with actual data
    JsonVariantConst result = doc["results"][0]["result"];
    if (result.isNull() || result.size() == 0) return false;
    
    // Try to access the first element
    JsonVariantConst firstElement = result[0];
    if (firstElement.isNull()) return false;
    
    // For non-nested structure, check if the first element has expected properties
    if (firstElement.containsKey("count") && firstElement.containsKey("order")) {
        return true;
    }
    
    // For nested structure, check the first item as an array
    if (firstElement.is<JsonArrayConst>()) {
        JsonArrayConst firstBreakdown = firstElement;
        if (firstBreakdown.isNull() || firstBreakdown.size() == 0) return false;
        
        JsonObjectConst firstStep = firstBreakdown[0];
        return !firstStep["order"].isNull() && !firstStep["count"].isNull();
    }
    
    // If we made it this far, check if the structure has breakdown_value (nested)
    if (!firstElement["breakdown_value"].isNull()) {
        return !firstElement["order"].isNull() && !firstElement["count"].isNull();
    }
    
    return false;
}

size_t InsightParser::getSeriesPointCount() const {
    if (!valid || !hasLineGraphStructure()) return 0;
    
    JsonArrayConst results = doc["results"];
    JsonArrayConst timeseriesData = results[0]["result"];
    return timeseriesData.size();
}

bool InsightParser::getSeriesYValues(double* yValues) const {
    if (!valid || !hasLineGraphStructure() || !yValues) return false;
    
    JsonArrayConst results = doc["results"];
    JsonArrayConst timeseriesData = results[0]["result"];
    size_t pointCount = timeseriesData.size();
    
    // Extract y-values directly - format is consistent with [date_string, numeric_value]
    for (size_t i = 0; i < pointCount; i++) {
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

size_t InsightParser::getFunnelBreakdownCount() const {
    if (!valid || !hasFunnelStructure()) return 0;
    
    // Check if we have a nested or flat structure
    bool isNested = hasFunnelNestedStructure();
    
    if (isNested) {
        // Original implementation for nested structure
        JsonArrayConst result = doc["results"][0]["result"];
        
        // Ensure the result is an array
        if (result.isNull()) {
            return 0;
        }
        
        return std::min(result.size(), (size_t)MAX_BREAKDOWNS);
    } else {
        // For flat structure, we always have exactly one breakdown
        return 1;
    }
}

size_t InsightParser::getFunnelStepCount() const {
    if (!valid || !hasFunnelStructure()) return 0;
    
    // For unpopulated funnels, count events and actions from filters
    if (!hasFunnelResultData()) {
        size_t count = 0;
        
        JsonArrayConst events = doc["results"][0]["filters"]["events"];
        if (!events.isNull()) {
            count += events.size();
        }
        
        JsonArrayConst actions = doc["results"][0]["filters"]["actions"];
        if (!actions.isNull()) {
            count += actions.size();
        }
        
        return count;
    }
    
    JsonArrayConst result = doc["results"][0]["result"];
    
    // For flat structure, count the items in the result array
    if (!hasFunnelNestedStructure()) {
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
) {
    if (!valid || !hasFunnelStructure()) return false;
    
    // Handle unpopulated funnels
    if (!hasFunnelResultData()) {
        // Handle null result case by retrieving metadata from filters
        if (breakdown_index > 0) return false; // Only support single breakdown for null result
        
        // Get combined list of events and actions
        JsonArrayConst events = doc["results"][0]["filters"]["events"];
        JsonArrayConst actions = doc["results"][0]["filters"]["actions"];
        
        size_t total_steps = 0;
        if (!events.isNull()) total_steps += events.size();
        if (!actions.isNull()) total_steps += actions.size();
        
        if (step_index >= total_steps) return false;
        
        // Determine if we need to get from events or actions
        JsonObjectConst step;
        if (step_index < (events.isNull() ? 0 : events.size())) {
            step = events[step_index];
        } else {
            step = actions[step_index - (events.isNull() ? 0 : events.size())];
        }
        
        // Get step name if buffer provided
        if (name_buffer && name_buffer_size > 0) {
            const char* custom_name = step["custom_name"];
            if (custom_name) {
                strncpy(name_buffer, custom_name, name_buffer_size - 1);
                name_buffer[name_buffer_size - 1] = '\0';
            } else {
                const char* name = step["name"];
                if (name) {
                    strncpy(name_buffer, name, name_buffer_size - 1);
                    name_buffer[name_buffer_size - 1] = '\0';
                }
            }
        }
        
        // For null result, we don't have count and conversion times
        if (count) *count = 0;
        if (conversion_time_avg) *conversion_time_avg = 0.0;
        if (conversion_time_median) *conversion_time_median = 0.0;
        
        return true;
    }
    
    JsonArrayConst result = doc["results"][0]["result"];
    JsonObjectConst step;
    
    // Handle flat structure
    if (!hasFunnelNestedStructure()) {
        // For flat structure, ignore breakdown_index (except 0)
        if (breakdown_index > 0) return false;
        
        // Check step_index is valid
        if (step_index >= result.size()) return false;
        
        // Get step directly from result
        step = result[step_index];
    } else {
        // Handle nested structure
        // Check breakdown_index is valid
        if (breakdown_index >= result.size()) return false;
        
        JsonArrayConst breakdown = result[breakdown_index];
        // Check step_index is valid
        if (step_index >= breakdown.size()) return false;
        
        // Get step from appropriate breakdown
        step = breakdown[step_index];
    }
    
    // Extract data from step (same for both structures)
    // Get step name if buffer provided
    if (name_buffer && name_buffer_size > 0) {
        const char* custom_name = step["custom_name"];
        if (custom_name) {
            strncpy(name_buffer, custom_name, name_buffer_size - 1);
            name_buffer[name_buffer_size - 1] = '\0';
        } else {
            const char* name = step["name"];
            if (name) {
                strncpy(name_buffer, name, name_buffer_size - 1);
                name_buffer[name_buffer_size - 1] = '\0';
            }
        }
    }
    
    // Get count
    if (count) {
        if (step["count"].isNull()) {
            *count = 0;
        } else {
            *count = step["count"].as<uint32_t>();
        }
    }
    
    // Get conversion times
    if (conversion_time_avg) {
        if (step["average_conversion_time"].isNull()) {
            *conversion_time_avg = 0.0;
        } else {
            *conversion_time_avg = step["average_conversion_time"].as<double>();
        }
    }
    
    if (conversion_time_median) {
        if (step["median_conversion_time"].isNull()) {
            *conversion_time_median = 0.0; 
        } else {
            *conversion_time_median = step["median_conversion_time"].as<double>();
        }
    }
    
    return true;
}

bool InsightParser::getFunnelBreakdownName(
    size_t breakdown_index,
    char* name_buffer,
    size_t buffer_size
) {
    if (!valid || !hasFunnelStructure()) return false;

    // Check if we have a nested or flat structure
    bool isNested = hasFunnelNestedStructure();

    if (isNested) {
        // Original implementation for nested structure
        JsonArrayConst result = doc["results"][0]["result"];

        // Ensure the result is an array
        if (result.isNull()) {
            return false;
        }

        if (breakdown_index >= result.size() || breakdown_index >= MAX_BREAKDOWNS) {
            return false;
        }

        // Get the breakdown name from the first step's breakdown field
        JsonArrayConst breakdown_array = result[breakdown_index];
        if (!breakdown_array.isNull() && breakdown_array.size() > 0 && 
            !breakdown_array[0]["breakdown"].isNull()) {
            
            JsonArrayConst breakdown = breakdown_array[0]["breakdown"];
            if (breakdown.size() > 0) {
                const char* value = breakdown[0].as<const char*>();
                if (value) {
                    strncpy(name_buffer, value, buffer_size);
                    name_buffer[buffer_size - 1] = '\0';
                    return true;
                }
            }
        }
    } else {
        // For flat structure, we only have one breakdown
        if (breakdown_index > 0) {
            return false;
        }
        
        // In flat structure, default to "All users"
        strncpy(name_buffer, "All users", buffer_size);
        name_buffer[buffer_size - 1] = '\0';
        return true;
    }

    return false;
}

bool InsightParser::getFunnelTotalCounts(
    size_t breakdown_index,
    uint32_t* counts,
    double* conversion_rates
) {
    if (!valid || !hasFunnelStructure()) return false;
    
    // Check if this is a flat or nested structure
    bool isNested = hasFunnelNestedStructure();
    size_t stepCount = getFunnelStepCount();
    
    if (stepCount == 0) {
        return false;
    }
    
    // For nested structure, we need to sum across all breakdowns for each step
    if (isNested) {
        // Handle nested structure
        JsonArrayConst result = doc["results"][0]["result"];
        
        if (result.isNull()) {
            return false;
        }
        
        // Clear the buffer first
        for (size_t i = 0; i < stepCount; i++) {
            counts[i] = 0;
        }
        
        // Go through each breakdown
        size_t breakdownCount = std::min(result.size(), (size_t)MAX_BREAKDOWNS);
        for (size_t breakdown = 0; breakdown < breakdownCount; breakdown++) {
            JsonArrayConst breakdown_array = result[breakdown];
            if (!breakdown_array.isNull()) {
                // For each step in this breakdown
                size_t stepsInBreakdown = std::min(breakdown_array.size(), stepCount);
                for (size_t step = 0; step < stepsInBreakdown; step++) {
                    if (!breakdown_array[step]["count"].isNull()) {
                        counts[step] += breakdown_array[step]["count"] | 0;
                    }
                }
            }
        }
    } else {
        // Simply copy from the flat structure directly
        JsonArrayConst result = doc["results"][0]["result"];
        
        if (result.isNull()) {
            return false;
        }
        
        // For each step
        for (size_t step = 0; step < stepCount; step++) {
            if (step < result.size() && !result[step]["count"].isNull()) {
                counts[step] = result[step]["count"] | 0;
            } else {
                counts[step] = 0;
            }
        }
    }
    
    // Calculate conversion rates if requested
    if (conversion_rates && counts[0] > 0) {
        for (size_t i = 0; i < stepCount - 1; i++) {
            if (counts[i] > 0) {
                conversion_rates[i] = (double)counts[i+1] / counts[i];
            } else {
                conversion_rates[i] = 0.0;
            }
        }
    }
    
    return true;
}

bool InsightParser::getFunnelConversionTimes(
    size_t breakdown_index,
    size_t step_index,
    double* avg_time,
    double* median_time
) {
    if (!valid || !hasFunnelStructure()) return false;
    if (step_index == 0) return false; // First step has no conversion time
    
    // For unpopulated funnels, we can't provide conversion times
    if (!hasFunnelResultData()) return false;
    
    // For flat structure
    if (!hasFunnelNestedStructure()) {
        // Only support breakdown_index 0 for flat structure
        if (breakdown_index > 0) return false;
        
        // Check step_index is valid
        JsonArrayConst result = doc["results"][0]["result"];
        if (step_index >= result.size()) return false;
        
        // Get conversion times directly from step
        if (avg_time) {
            if (result[step_index]["average_conversion_time"].isNull()) {
                *avg_time = 0.0;
            } else {
                *avg_time = result[step_index]["average_conversion_time"].as<double>();
            }
        }
        if (median_time) {
            if (result[step_index]["median_conversion_time"].isNull()) {
                *median_time = 0.0;
            } else {
                *median_time = result[step_index]["median_conversion_time"].as<double>();
            }
        }
        
        return true;
    }
    
    // For nested structure, use existing function
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
) {
    if (!valid || !hasFunnelStructure()) return false;
    
    // For unpopulated funnels, get metadata from filters
    if (!hasFunnelResultData()) {
        JsonArrayConst events = doc["results"][0]["filters"]["events"];
        JsonArrayConst actions = doc["results"][0]["filters"]["actions"];
        
        size_t total_steps = 0;
        if (!events.isNull()) total_steps += events.size();
        if (!actions.isNull()) total_steps += actions.size();
        
        if (step_index >= total_steps) return false;
        
        // Determine if we need to get from events or actions
        JsonObjectConst step;
        if (step_index < (events.isNull() ? 0 : events.size())) {
            step = events[step_index];
        } else {
            step = actions[step_index - (events.isNull() ? 0 : events.size())];
        }
        
        // Get custom name
        if (custom_name_buffer && name_buffer_size > 0) {
            const char* custom_name = step["custom_name"];
            if (custom_name) {
                strncpy(custom_name_buffer, custom_name, name_buffer_size - 1);
                custom_name_buffer[name_buffer_size - 1] = '\0';
            }
        }
        
        // Get action ID
        if (action_id_buffer && action_buffer_size > 0) {
            const char* action_id = step["id"];
            if (action_id) {
                strncpy(action_id_buffer, action_id, action_buffer_size - 1);
                action_id_buffer[action_buffer_size - 1] = '\0';
            }
        }
        
        return true;
    }
    
    JsonVariantConst result = doc["results"][0]["result"];
    JsonObjectConst step;
    
    // For flat structure
    if (!hasFunnelNestedStructure()) {
        if (step_index >= result.size()) return false;
        step = result[step_index];
    } else {
        // For nested structure, use the first breakdown
        JsonArrayConst firstBreakdown = result[0];
        if (step_index >= firstBreakdown.size()) return false;
        step = firstBreakdown[step_index];
    }
    
    // Get custom name
    if (custom_name_buffer && name_buffer_size > 0) {
        const char* custom_name = step["custom_name"];
        if (custom_name) {
            strncpy(custom_name_buffer, custom_name, name_buffer_size - 1);
            custom_name_buffer[name_buffer_size - 1] = '\0';
        }
    }
    
    // Get action ID
    if (action_id_buffer && action_buffer_size > 0) {
        const char* action_id = step["action_id"];
        if (action_id) {
            strncpy(action_id_buffer, action_id, action_buffer_size - 1);
            action_id_buffer[action_buffer_size - 1] = '\0';
        }
    }
    
    return true;
}

bool InsightParser::getFunnelBreakdownComparison(
    size_t step_index,
    uint32_t* counts,
    double* conversion_rates
) {
    if (!valid || !hasFunnelStructure()) return false;

    // Check if this is a flat or nested structure
    bool isNested = hasFunnelNestedStructure();

    if (isNested) {
        // Original implementation for nested structure
        JsonArrayConst result = doc["results"][0]["result"];

        // Ensure the result is an array
        if (result.isNull()) {
            return false;
        }

        size_t breakdownCount = result.size();
        if (breakdownCount == 0) {
            return false;
        }

        // For each breakdown, get the count for this step
        for (size_t breakdown_index = 0; breakdown_index < breakdownCount; breakdown_index++) {
            if (breakdown_index >= MAX_BREAKDOWNS) break;

            JsonArrayConst breakdown_array = result[breakdown_index];
            if (!breakdown_array.isNull() && step_index < breakdown_array.size()) {
                
                JsonObjectConst stepObj = breakdown_array[step_index];
                
                // Extract count
                if (!stepObj["count"].isNull()) {
                    counts[breakdown_index] = stepObj["count"] | 0;
                } else {
                    counts[breakdown_index] = 0;
                }
                
                // Calculate conversion rate compared to first step
                if (conversion_rates && breakdown_index < result.size() && 
                    breakdown_array.size() > 0 && 
                    !breakdown_array[0]["count"].isNull()) {
                    
                    uint32_t firstStepCount = breakdown_array[0]["count"] | 0;
                    if (firstStepCount > 0) {
                        conversion_rates[breakdown_index] = (double)counts[breakdown_index] / firstStepCount;
                    } else {
                        conversion_rates[breakdown_index] = 0.0;
                    }
                } else if (conversion_rates) {
                    conversion_rates[breakdown_index] = 0.0;
                }
            } else {
                counts[breakdown_index] = 0;
                if (conversion_rates) {
                    conversion_rates[breakdown_index] = 0.0;
                }
            }
        }
    } else {
        // Flat structure implementation
        JsonArrayConst result = doc["results"][0]["result"];
        
        if (result.isNull() || step_index >= result.size()) {
            return false;
        }
        
        // In flat structure, we only have one breakdown
        counts[0] = result[step_index]["count"] | 0;
        
        // Calculate conversion rate compared to first step
        if (conversion_rates && result.size() > 0 && !result[0]["count"].isNull()) {
            uint32_t firstStepCount = result[0]["count"] | 0;
            if (firstStepCount > 0) {
                conversion_rates[0] = (double)counts[0] / firstStepCount;
            } else {
                conversion_rates[0] = 0.0;
            }
        } else if (conversion_rates) {
            conversion_rates[0] = 0.0;
        }
        
        // Zero out any additional breakdown slots
        for (size_t i = 1; i < MAX_BREAKDOWNS; i++) {
            counts[i] = 0;
            if (conversion_rates) {
                conversion_rates[i] = 0.0;
            }
        }
    }
    
    return true;
}

bool InsightParser::getFunnelTimeWindow(uint32_t* window_days) {
    if (!valid || !hasFunnelStructure() || !window_days) return false;
    
    // Always check filters first since both formats have this
    JsonVariantConst filters = doc["results"][0]["filters"];
    if (!filters.isNull()) {
        uint32_t interval = filters["funnel_window_interval"] | 0;
        if (interval > 0) {
            // Check interval unit
            const char* unit = filters["funnel_window_interval_unit"];
            if (unit) {
                if (strcmp(unit, "day") == 0) {
                    *window_days = interval;
                    return true;
                } else if (strcmp(unit, "week") == 0) {
                    *window_days = interval * 7;
                    return true;
                } else if (strcmp(unit, "month") == 0) {
                    *window_days = interval * 30;  // Approximate
                    return true;
                }
            }
            
            // Default to days if no unit specified
            *window_days = interval;
            return true;
        }
    }
    
    return false;
}

bool InsightParser::hasFunnelNestedStructure() const {
    if (!valid || !hasFunnelStructure() || !hasFunnelResultData()) return false;
    
    // Examine the result structure to determine if it's nested or flat
    JsonVariantConst result = doc["results"][0]["result"];
    JsonVariantConst firstElement = result[0];
    
    // If first element is an array, it's definitely a nested structure
    if (firstElement.is<JsonArrayConst>()) {
        return true;
    }
    
    // If the first element has breakdown_value array, it's a nested structure
    if (!firstElement["breakdown_value"].isNull()) {
        return true;
    }
    
    // For flat structure, usually there are step completions like "Completed X steps"
    // or no breakdown_value property
    return false;
} 