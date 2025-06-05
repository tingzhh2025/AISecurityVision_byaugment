#!/bin/bash

# AI Security Vision API Test Runner
# ==================================
# This script runs comprehensive API endpoint tests for the AI Security Vision system

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
BOLD='\033[1m'
NC='\033[0m' # No Color

# Configuration
BASE_URL="http://localhost:8080"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

echo -e "${BOLD}${BLUE}AI Security Vision API Test Suite${NC}"
echo -e "${BLUE}=================================${NC}"
echo ""

# Check if backend service is running
echo -e "${CYAN}🔍 Checking backend service...${NC}"
if curl -s --connect-timeout 5 "$BASE_URL/api/system/status" > /dev/null 2>&1; then
    echo -e "${GREEN}✅ Backend service is running at $BASE_URL${NC}"
else
    echo -e "${RED}❌ Backend service is not accessible at $BASE_URL${NC}"
    echo -e "${YELLOW}Please ensure the AISecurityVision service is running:${NC}"
    echo -e "  cd $PROJECT_ROOT/build"
    echo -e "  ./AISecurityVision"
    exit 1
fi

# Check Python dependencies
echo -e "${CYAN}🔍 Checking Python dependencies...${NC}"
if ! command -v python3 &> /dev/null; then
    echo -e "${RED}❌ Python3 is not installed${NC}"
    exit 1
fi

# Install required Python packages if needed
python3 -c "import requests" 2>/dev/null || {
    echo -e "${YELLOW}⚠️  Installing required Python packages...${NC}"
    pip3 install requests --user
}

# Run the comprehensive Python test script
echo -e "${CYAN}🧪 Running comprehensive API tests...${NC}"
cd "$SCRIPT_DIR"
python3 api_endpoint_tester.py

# Check if reports were generated
if [ -f "api_test_report.html" ] && [ -f "api_test_report.md" ]; then
    echo -e "${GREEN}✅ Test reports generated successfully!${NC}"
    echo -e "  📄 HTML Report: $SCRIPT_DIR/api_test_report.html"
    echo -e "  📄 Markdown Report: $SCRIPT_DIR/api_test_report.md"
    
    # Try to open HTML report in browser (optional)
    if command -v xdg-open &> /dev/null; then
        echo -e "${CYAN}🌐 Opening HTML report in browser...${NC}"
        xdg-open "api_test_report.html" 2>/dev/null || true
    fi
else
    echo -e "${YELLOW}⚠️  Reports may not have been generated properly${NC}"
fi

echo ""
echo -e "${BOLD}${GREEN}🎉 API testing completed!${NC}"
