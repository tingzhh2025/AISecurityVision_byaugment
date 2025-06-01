#!/bin/bash

# AI Security Vision API Test Summary Generator
# Generates a comprehensive summary of API consistency test results

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
PURPLE='\033[0;35m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

echo -e "${BLUE}📊 AI Security Vision API Test Summary${NC}"
echo "=============================================="

# Function to check if file exists and is readable
check_file() {
    local file=$1
    if [ -f "$file" ] && [ -r "$file" ]; then
        return 0
    else
        return 1
    fi
}

# Function to parse JSON results
parse_json_results() {
    local json_file=$1
    
    if ! check_file "$json_file"; then
        echo -e "${RED}❌ JSON results file not found: $json_file${NC}"
        return 1
    fi
    
    echo -e "${CYAN}📋 Test Results Summary:${NC}"
    
    # Extract basic statistics using grep and basic text processing
    local total_tests=$(grep -o '"title"' "$json_file" | wc -l)
    local passed_tests=$(grep -o '"status":"passed"' "$json_file" | wc -l)
    local failed_tests=$(grep -o '"status":"failed"' "$json_file" | wc -l)
    local skipped_tests=$(grep -o '"status":"skipped"' "$json_file" | wc -l)
    
    echo "   Total Tests: $total_tests"
    echo -e "   ${GREEN}Passed: $passed_tests${NC}"
    
    if [ "$failed_tests" -gt 0 ]; then
        echo -e "   ${RED}Failed: $failed_tests${NC}"
    else
        echo -e "   ${GREEN}Failed: $failed_tests${NC}"
    fi
    
    if [ "$skipped_tests" -gt 0 ]; then
        echo -e "   ${YELLOW}Skipped: $skipped_tests${NC}"
    else
        echo -e "   ${GREEN}Skipped: $skipped_tests${NC}"
    fi
    
    # Calculate success rate
    if [ "$total_tests" -gt 0 ]; then
        local success_rate=$((passed_tests * 100 / total_tests))
        echo "   Success Rate: ${success_rate}%"
    fi
    
    echo ""
}

# Function to show test categories
show_test_categories() {
    echo -e "${PURPLE}🧪 Test Categories Covered:${NC}"
    echo ""
    
    echo -e "${BLUE}1. System Endpoints${NC}"
    echo "   ✓ GET /api/system/status"
    echo "   ✓ GET /api/system/info"
    echo "   ✓ GET /api/system/config"
    echo "   ✓ GET /api/system/metrics"
    echo "   ✓ GET /api/system/stats"
    echo ""
    
    echo -e "${BLUE}2. Camera Management${NC}"
    echo "   ✓ GET /api/cameras"
    echo "   ✓ POST /api/cameras"
    echo "   ✓ PUT /api/cameras/{id}"
    echo "   ✓ DELETE /api/cameras/{id}"
    echo "   ✓ POST /api/cameras/test-connection"
    echo ""
    
    echo -e "${BLUE}3. Camera Configuration${NC}"
    echo "   ✓ GET /api/cameras/config"
    echo "   ✓ POST /api/cameras/config"
    echo "   ✓ DELETE /api/cameras/config/{id}"
    echo ""
    
    echo -e "${BLUE}4. Alert Management${NC}"
    echo "   ✓ GET /api/alerts"
    echo "   ✓ PUT /api/alerts/{id}/read"
    echo "   ✓ DELETE /api/alerts/{id}"
    echo ""
    
    echo -e "${BLUE}5. Detection Configuration${NC}"
    echo "   ✓ GET /api/detection/config"
    echo "   ✓ PUT /api/detection/config"
    echo ""
    
    echo -e "${BLUE}6. Person Statistics${NC}"
    echo "   ✓ GET /api/cameras/{id}/person-stats"
    echo "   ✓ POST /api/cameras/{id}/person-stats/enable"
    echo "   ✓ POST /api/cameras/{id}/person-stats/disable"
    echo "   ✓ GET /api/cameras/{id}/person-stats/config"
    echo "   ✓ POST /api/cameras/{id}/person-stats/config"
    echo ""
    
    echo -e "${BLUE}7. Data Validation${NC}"
    echo "   ✓ Camera object structure validation"
    echo "   ✓ System status response validation"
    echo "   ✓ Alert data structure validation"
    echo "   ✓ Error response format validation"
    echo ""
    
    echo -e "${BLUE}8. Error Handling${NC}"
    echo "   ✓ 404 error consistency"
    echo "   ✓ 400 validation error consistency"
    echo "   ✓ Response format consistency"
    echo "   ✓ CORS header consistency"
    echo ""
}

# Function to show recommendations
show_recommendations() {
    echo -e "${YELLOW}💡 Recommendations:${NC}"
    echo ""
    
    if check_file "reports/results.json"; then
        local failed_count=$(grep -o '"status":"failed"' "reports/results.json" | wc -l)
        
        if [ "$failed_count" -gt 0 ]; then
            echo -e "${RED}⚠️ Failed Tests Detected${NC}"
            echo "   1. Review the HTML report for detailed failure information"
            echo "   2. Check backend and frontend logs for errors"
            echo "   3. Verify API endpoint implementations match specifications"
            echo "   4. Ensure data structures are consistent between frontend and backend"
            echo ""
        fi
    fi
    
    echo -e "${GREEN}✅ Best Practices${NC}"
    echo "   1. Run tests regularly during development"
    echo "   2. Update tests when adding new API endpoints"
    echo "   3. Maintain consistent error handling patterns"
    echo "   4. Keep frontend API service in sync with backend changes"
    echo "   5. Use the test results to identify integration issues early"
    echo ""
}

# Function to show next steps
show_next_steps() {
    echo -e "${CYAN}🚀 Next Steps:${NC}"
    echo ""
    
    echo "1. View detailed HTML report:"
    echo "   npx playwright show-report"
    echo ""
    
    echo "2. Run specific test categories:"
    echo "   npm run test:consistency    # API endpoint consistency"
    echo "   npm run test:validation     # Data validation tests"
    echo ""
    
    echo "3. Debug failing tests:"
    echo "   npm run test:debug          # Interactive debugging"
    echo "   npm run test:headed         # Visual browser testing"
    echo ""
    
    echo "4. Integration with CI/CD:"
    echo "   - Use reports/results.xml for JUnit integration"
    echo "   - Use reports/results.json for custom processing"
    echo "   - Archive reports/html/ for detailed analysis"
    echo ""
}

# Main execution
main() {
    # Check if we're in the right directory
    if [ ! -f "playwright.config.js" ]; then
        echo -e "${RED}❌ playwright.config.js not found${NC}"
        echo -e "${YELLOW}   Please run this script from the tests/playwright directory${NC}"
        exit 1
    fi
    
    # Show test categories
    show_test_categories
    
    # Parse and show results if available
    if check_file "reports/results.json"; then
        parse_json_results "reports/results.json"
    else
        echo -e "${YELLOW}⚠️ No test results found. Run tests first with: ./run-tests.sh${NC}"
        echo ""
    fi
    
    # Show recommendations
    show_recommendations
    
    # Show next steps
    show_next_steps
    
    # Show file locations
    echo -e "${BLUE}📁 Important Files:${NC}"
    echo "   Configuration: playwright.config.js"
    echo "   Test Files: api-consistency.spec.js, api-data-validation.spec.js"
    echo "   Test Runner: run-tests.sh"
    echo "   Documentation: README.md"
    
    if check_file "reports/results.json"; then
        echo "   JSON Results: reports/results.json"
    fi
    
    if check_file "reports/results.xml"; then
        echo "   JUnit Results: reports/results.xml"
    fi
    
    if [ -d "reports/html" ]; then
        echo "   HTML Report: reports/html/index.html"
    fi
    
    echo ""
    echo -e "${GREEN}✅ API consistency testing framework is ready!${NC}"
}

# Parse command line arguments
case "${1:-}" in
    --help|-h)
        echo "Usage: $0 [options]"
        echo ""
        echo "Options:"
        echo "  --help, -h     Show this help message"
        echo ""
        echo "This script generates a summary of API consistency test results."
        echo "Run ./run-tests.sh first to generate test results."
        exit 0
        ;;
    *)
        main
        ;;
esac
