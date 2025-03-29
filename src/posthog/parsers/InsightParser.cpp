#include "InsightParser.h"
#include <stdio.h>

InsightParser::InsightParser(const char* json) : doc(262144), valid(false) {  // 256KB
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
    
    // Check for basic funnel structure
    JsonArrayConst results = doc["results"];
    if (results.isNull() || results.size() == 0) return false;
    
    // Check for result array containing funnel steps
    JsonArrayConst result = results[0]["result"];
    if (result.isNull() || result.size() == 0) return false;
    
    // Verify first breakdown has steps array
    JsonArrayConst firstBreakdown = result[0];
    if (firstBreakdown.isNull() || firstBreakdown.size() == 0) return false;
    
    // Check if first step has expected funnel properties
    JsonObjectConst firstStep = firstBreakdown[0];
    return !firstStep["order"].isNull() && !firstStep["count"].isNull();
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
    
    JsonArrayConst result = doc["results"][0]["result"];
    return result.size();  // Each element in result is a breakdown
}

size_t InsightParser::getFunnelStepCount() const {
    if (!valid || !hasFunnelStructure()) return 0;
    
    JsonArrayConst firstBreakdown = doc["results"][0]["result"][0];
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
    
    JsonArrayConst result = doc["results"][0]["result"];
    if (breakdown_index >= result.size()) return false;
    
    JsonArrayConst breakdown = result[breakdown_index];
    if (step_index >= breakdown.size()) return false;
    
    JsonObjectConst step = breakdown[step_index];
    
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
        *count = step["count"] | 0;
    }
    
    // Get conversion times
    if (conversion_time_avg) {
        *conversion_time_avg = step["average_conversion_time"] | 0.0;
    }
    
    if (conversion_time_median) {
        *conversion_time_median = step["median_conversion_time"] | 0.0;
    }
    
    return true;
}

bool InsightParser::getFunnelBreakdownName(
    size_t breakdown_index,
    char* buffer,
    size_t buffer_size
) {
    if (!valid || !hasFunnelStructure() || !buffer || buffer_size == 0) return false;
    
    JsonArrayConst result = doc["results"][0]["result"];
    if (breakdown_index >= result.size()) return false;
    
    JsonArrayConst breakdown = result[breakdown_index];
    if (breakdown.size() == 0) return false;
    
    JsonVariantConst breakdown_value = breakdown[0]["breakdown_value"][0];
    if (breakdown_value.isNull()) return false;
    
    const char* value = breakdown_value.as<const char*>();
    if (!value) return false;
    
    strncpy(buffer, value, buffer_size - 1);
    buffer[buffer_size - 1] = '\0';
    return true;
}

bool InsightParser::getFunnelTotalCounts(
    size_t breakdown_index,
    uint32_t* counts_array,
    double* conversion_rates
) {
    if (!valid || !hasFunnelStructure() || !counts_array) return false;
    
    JsonArrayConst result = doc["results"][0]["result"];
    if (breakdown_index >= result.size()) return false;
    
    JsonArrayConst breakdown = result[breakdown_index];
    size_t step_count = breakdown.size();
    
    // Get counts for each step
    for (size_t i = 0; i < step_count; i++) {
        counts_array[i] = breakdown[i]["count"] | 0;
    }
    
    // Calculate conversion rates between steps
    if (conversion_rates) {
        for (size_t i = 0; i < step_count - 1; i++) {
            if (counts_array[i] > 0) {
                conversion_rates[i] = (double)counts_array[i + 1] / counts_array[i];
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
    
    JsonArrayConst firstBreakdown = doc["results"][0]["result"][0];
    if (step_index >= firstBreakdown.size()) return false;
    
    JsonObjectConst step = firstBreakdown[step_index];
    
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
    
    JsonArrayConst result = doc["results"][0]["result"];
    size_t breakdown_count = result.size();
    
    for (size_t i = 0; i < breakdown_count; i++) {
        JsonArrayConst breakdown = result[i];
        if (step_index >= breakdown.size()) return false;
        
        counts[i] = breakdown[step_index]["count"] | 0;
        
        if (conversion_rates) {
            if (step_index > 0 && breakdown[0]["count"].as<uint32_t>() > 0) {
                conversion_rates[i] = (double)counts[i] / breakdown[0]["count"].as<uint32_t>();
            } else {
                conversion_rates[i] = 0.0;
            }
        }
    }
    
    return true;
}

bool InsightParser::getFunnelTimeWindow(uint32_t* window_days) {
    if (!valid || !hasFunnelStructure() || !window_days) return false;
    
    JsonVariantConst filters = doc["results"][0]["filters"];
    if (filters.isNull()) return false;
    
    *window_days = filters["funnel_window_interval"] | 0;
    return true;
} 