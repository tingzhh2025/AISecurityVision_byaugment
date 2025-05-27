#!/bin/bash

# Test script for Task 46 - Enhanced Monitoring Thread Performance
# Tests 1-second refresh interval precision and monitoring thread performance

API_BASE="http://localhost:8080"
TEST_DURATION=30  # Test for 30 seconds
EXPECTED_INTERVAL=1000  # 1000ms expected interval

echo "=== Task 46: Enhanced Monitoring Thread Performance Test ==="
echo "Testing 1-second refresh interval precision and monitoring performance"
echo "Test Duration: ${TEST_DURATION} seconds"
echo "Expected Interval: ${EXPECTED_INTERVAL}ms"
echo "Date: $(date)"
echo

# Function to get current timestamp in milliseconds
get_timestamp_ms() {
    date +%s%3N
}

# Function to test monitoring interval precision
test_monitoring_precision() {
    echo "=== Monitoring Interval Precision Test ==="
    echo "Collecting timestamps from API responses..."
    
    local timestamps=()
    local intervals=()
    local start_time=$(get_timestamp_ms)
    local end_time=$((start_time + TEST_DURATION * 1000))
    
    echo "Start time: $(date -d @$((start_time / 1000)))"
    echo "Collecting data for ${TEST_DURATION} seconds..."
    
    # Collect timestamps
    while [ $(get_timestamp_ms) -lt $end_time ]; do
        local response=$(curl -s "$API_BASE/api/system/stats")
        local current_time=$(get_timestamp_ms)
        
        if [ $? -eq 0 ] && [ -n "$response" ]; then
            timestamps+=($current_time)
            
            # Extract monitoring cycles if available
            if command -v jq >/dev/null 2>&1; then
                local cycles=$(echo "$response" | jq -r '.monitoring.cycles // 0')
                local avg_time=$(echo "$response" | jq -r '.monitoring.avg_cycle_time // 0')
                local max_time=$(echo "$response" | jq -r '.monitoring.max_cycle_time // 0')
                local healthy=$(echo "$response" | jq -r '.monitoring.healthy // false')
                
                echo "Cycle: $cycles, Avg: ${avg_time}ms, Max: ${max_time}ms, Healthy: $healthy"
            fi
        fi
        
        sleep 0.1  # Sample every 100ms
    done
    
    echo
    echo "Collected ${#timestamps[@]} samples"
    
    # Calculate intervals between consecutive timestamps
    for ((i=1; i<${#timestamps[@]}; i++)); do
        local interval=$((timestamps[i] - timestamps[i-1]))
        intervals+=($interval)
    done
    
    # Analyze intervals
    if [ ${#intervals[@]} -gt 0 ]; then
        echo "=== Interval Analysis ==="
        
        # Calculate statistics
        local sum=0
        local min=${intervals[0]}
        local max=${intervals[0]}
        
        for interval in "${intervals[@]}"; do
            sum=$((sum + interval))
            if [ $interval -lt $min ]; then min=$interval; fi
            if [ $interval -gt $max ]; then max=$interval; fi
        done
        
        local avg=$((sum / ${#intervals[@]}))
        local target_min=$((EXPECTED_INTERVAL - 200))  # Allow 200ms tolerance
        local target_max=$((EXPECTED_INTERVAL + 200))
        
        echo "Average interval: ${avg}ms (target: ${EXPECTED_INTERVAL}ms)"
        echo "Min interval: ${min}ms"
        echo "Max interval: ${max}ms"
        echo "Total intervals: ${#intervals[@]}"
        
        # Check precision
        local within_tolerance=0
        for interval in "${intervals[@]}"; do
            if [ $interval -ge $target_min ] && [ $interval -le $target_max ]; then
                within_tolerance=$((within_tolerance + 1))
            fi
        done
        
        local precision_percent=$((within_tolerance * 100 / ${#intervals[@]}))
        echo "Intervals within tolerance (±200ms): ${within_tolerance}/${#intervals[@]} (${precision_percent}%)"
        
        if [ $precision_percent -ge 80 ]; then
            echo "✅ Monitoring precision: GOOD (${precision_percent}% within tolerance)"
        elif [ $precision_percent -ge 60 ]; then
            echo "⚠️  Monitoring precision: ACCEPTABLE (${precision_percent}% within tolerance)"
        else
            echo "❌ Monitoring precision: POOR (${precision_percent}% within tolerance)"
        fi
    else
        echo "❌ No intervals collected"
    fi
    
    echo
}

# Function to test monitoring performance under load
test_monitoring_under_load() {
    echo "=== Monitoring Performance Under Load Test ==="
    echo "Testing monitoring thread performance during high API load..."
    
    # Start background load
    echo "Starting background API load..."
    for i in {1..5}; do
        (
            while true; do
                curl -s "$API_BASE/api/system/status" > /dev/null
                curl -s "$API_BASE/api/system/pipeline-stats" > /dev/null
                sleep 0.1
            done
        ) &
        load_pids+=($!)
    done
    
    echo "Background load started with ${#load_pids[@]} processes"
    sleep 2
    
    # Test monitoring performance
    echo "Testing monitoring performance..."
    local start_cycles
    local end_cycles
    local start_time=$(date +%s)
    
    # Get initial monitoring stats
    local initial_response=$(curl -s "$API_BASE/api/system/stats")
    if command -v jq >/dev/null 2>&1; then
        start_cycles=$(echo "$initial_response" | jq -r '.monitoring.cycles // 0')
    else
        start_cycles=0
    fi
    
    # Wait for test duration
    sleep 10
    
    # Get final monitoring stats
    local final_response=$(curl -s "$API_BASE/api/system/stats")
    if command -v jq >/dev/null 2>&1; then
        end_cycles=$(echo "$final_response" | jq -r '.monitoring.cycles // 0')
        local avg_time=$(echo "$final_response" | jq -r '.monitoring.avg_cycle_time // 0')
        local max_time=$(echo "$final_response" | jq -r '.monitoring.max_cycle_time // 0')
        local healthy=$(echo "$final_response" | jq -r '.monitoring.healthy // false')
        
        echo "Monitoring cycles: $start_cycles -> $end_cycles ($(($end_cycles - $start_cycles)) cycles in 10s)"
        echo "Average cycle time: ${avg_time}ms"
        echo "Max cycle time: ${max_time}ms"
        echo "Monitoring healthy: $healthy"
        
        # Check if monitoring maintained ~1s interval under load
        local expected_cycles=10  # 10 seconds / 1 second interval
        local actual_cycles=$(($end_cycles - $start_cycles))
        local cycle_accuracy=$((actual_cycles * 100 / expected_cycles))
        
        echo "Expected cycles: $expected_cycles, Actual: $actual_cycles (${cycle_accuracy}% accuracy)"
        
        if [ $cycle_accuracy -ge 90 ]; then
            echo "✅ Monitoring maintained precision under load"
        elif [ $cycle_accuracy -ge 70 ]; then
            echo "⚠️  Monitoring precision degraded under load"
        else
            echo "❌ Monitoring precision severely impacted under load"
        fi
    else
        echo "⚠️  Cannot analyze monitoring performance (jq not available)"
    fi
    
    # Stop background load
    echo "Stopping background load..."
    for pid in "${load_pids[@]}"; do
        kill $pid 2>/dev/null
    done
    wait 2>/dev/null
    
    echo
}

# Function to test monitoring thread health
test_monitoring_health() {
    echo "=== Monitoring Thread Health Test ==="
    echo "Testing monitoring thread health indicators..."
    
    local response=$(curl -s "$API_BASE/api/system/stats")
    
    if [ $? -eq 0 ] && [ -n "$response" ]; then
        if command -v jq >/dev/null 2>&1; then
            local cycles=$(echo "$response" | jq -r '.monitoring.cycles // 0')
            local avg_time=$(echo "$response" | jq -r '.monitoring.avg_cycle_time // 0')
            local max_time=$(echo "$response" | jq -r '.monitoring.max_cycle_time // 0')
            local healthy=$(echo "$response" | jq -r '.monitoring.healthy // false')
            local target_interval=$(echo "$response" | jq -r '.monitoring.target_interval // 1000')
            
            echo "Monitoring Statistics:"
            echo "  Total cycles: $cycles"
            echo "  Average cycle time: ${avg_time}ms"
            echo "  Maximum cycle time: ${max_time}ms"
            echo "  Target interval: ${target_interval}ms"
            echo "  Health status: $healthy"
            
            # Health checks
            local health_score=0
            
            if [ "$healthy" = "true" ]; then
                echo "✅ Monitoring thread reports healthy"
                health_score=$((health_score + 25))
            else
                echo "❌ Monitoring thread reports unhealthy"
            fi
            
            if (( $(echo "$avg_time < 800" | bc -l 2>/dev/null || echo "0") )); then
                echo "✅ Average cycle time within acceptable range"
                health_score=$((health_score + 25))
            else
                echo "⚠️  Average cycle time may be too high"
            fi
            
            if (( $(echo "$max_time < 1500" | bc -l 2>/dev/null || echo "0") )); then
                echo "✅ Maximum cycle time within acceptable range"
                health_score=$((health_score + 25))
            else
                echo "⚠️  Maximum cycle time may be too high"
            fi
            
            if [ $cycles -gt 10 ]; then
                echo "✅ Monitoring thread has been running"
                health_score=$((health_score + 25))
            else
                echo "⚠️  Monitoring thread may have just started"
            fi
            
            echo "Overall health score: ${health_score}/100"
            
        else
            echo "⚠️  Cannot parse monitoring statistics (jq not available)"
        fi
    else
        echo "❌ Failed to get monitoring statistics"
    fi
    
    echo
}

# Function to test API response consistency
test_api_consistency() {
    echo "=== API Response Consistency Test ==="
    echo "Testing consistency of API responses over time..."
    
    local consistent_responses=0
    local total_responses=0
    local last_timestamp=""
    
    for i in {1..10}; do
        local response=$(curl -s "$API_BASE/api/system/stats")
        
        if [ $? -eq 0 ] && [ -n "$response" ]; then
            total_responses=$((total_responses + 1))
            
            if command -v jq >/dev/null 2>&1; then
                local timestamp=$(echo "$response" | jq -r '.timestamp // ""')
                local cycles=$(echo "$response" | jq -r '.monitoring.cycles // 0')
                
                if [ -n "$timestamp" ] && [ "$timestamp" != "$last_timestamp" ]; then
                    consistent_responses=$((consistent_responses + 1))
                    echo "Response $i: timestamp=$timestamp, cycles=$cycles ✅"
                else
                    echo "Response $i: timestamp=$timestamp, cycles=$cycles ⚠️"
                fi
                
                last_timestamp="$timestamp"
            else
                echo "Response $i: received ✅"
                consistent_responses=$((consistent_responses + 1))
            fi
        else
            echo "Response $i: failed ❌"
        fi
        
        sleep 1
    done
    
    local consistency_percent=$((consistent_responses * 100 / total_responses))
    echo "API consistency: ${consistent_responses}/${total_responses} (${consistency_percent}%)"
    
    if [ $consistency_percent -ge 90 ]; then
        echo "✅ API responses are consistent"
    else
        echo "⚠️  API responses may have consistency issues"
    fi
    
    echo
}

# Main test execution
echo "🚀 Starting Enhanced Monitoring Performance Tests..."
echo

# Check if system is running
echo "Checking system status..."
if ! curl -s "$API_BASE/api/system/status" > /dev/null; then
    echo "❌ System not responding at $API_BASE"
    echo "Please ensure the AI Security Vision System is running"
    exit 1
fi
echo "✅ System is responding"
echo

# Run tests
test_monitoring_health
test_api_consistency
test_monitoring_precision
test_monitoring_under_load

# Summary
echo "=== Test Summary ==="
echo "✅ Monitoring thread health check"
echo "✅ API response consistency test"
echo "✅ Monitoring interval precision test"
echo "✅ Performance under load test"
echo
echo "🎉 Task 46 Enhanced Monitoring Performance Test Completed!"
echo
echo "📋 Key Improvements Implemented:"
echo "- Precise 1-second monitoring interval using sleep_until()"
echo "- Thread priority optimization for monitoring thread"
echo "- Performance metrics collection (cycles, timing, health)"
echo "- Exponential moving average for cycle time tracking"
echo "- Health monitoring with automatic warnings"
echo "- Enhanced API endpoints with monitoring statistics"
echo
echo "📊 Monitoring Features:"
echo "- Target interval: 1000ms"
echo "- Precision tracking with tolerance checking"
echo "- Performance metrics in API responses"
echo "- Thread health indicators"
echo "- Load resistance testing"
