#!/bin/bash

# AI Security Vision API Consistency Test Runner
# This script runs comprehensive API consistency tests between frontend and backend

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Configuration
BACKEND_PORT=8080
FRONTEND_PORT=3000
TEST_TIMEOUT=300
BACKEND_BINARY="../../build/AISecurityVision"
FRONTEND_DIR="../../web-ui"

echo -e "${BLUE}🚀 AI Security Vision API Consistency Test Runner${NC}"
echo "=================================================="

# Function to check if port is in use
check_port() {
    local port=$1
    if lsof -Pi :$port -sTCP:LISTEN -t >/dev/null 2>&1; then
        return 0
    else
        return 1
    fi
}

# Function to wait for service
wait_for_service() {
    local url=$1
    local name=$2
    local timeout=$3
    
    echo -e "${YELLOW}⏳ Waiting for $name to be ready...${NC}"
    
    local count=0
    while [ $count -lt $timeout ]; do
        if curl -s "$url" >/dev/null 2>&1; then
            echo -e "${GREEN}✅ $name is ready${NC}"
            return 0
        fi
        sleep 2
        count=$((count + 2))
    done
    
    echo -e "${RED}❌ $name failed to start within $timeout seconds${NC}"
    return 1
}

# Function to start backend
start_backend() {
    echo -e "${BLUE}🔧 Starting backend service...${NC}"
    
    if check_port $BACKEND_PORT; then
        echo -e "${YELLOW}⚠️ Backend port $BACKEND_PORT is already in use${NC}"
        echo -e "${YELLOW}   Assuming backend is already running${NC}"
    else
        if [ ! -f "$BACKEND_BINARY" ]; then
            echo -e "${RED}❌ Backend binary not found: $BACKEND_BINARY${NC}"
            echo -e "${YELLOW}   Please build the project first: make build${NC}"
            exit 1
        fi
        
        echo -e "${BLUE}   Starting $BACKEND_BINARY...${NC}"
        cd ../../
        nohup ./build/AISecurityVision --config config/system.json > tests/playwright/backend.log 2>&1 &
        BACKEND_PID=$!
        cd tests/playwright/
        
        echo "Backend PID: $BACKEND_PID" > backend.pid
        
        # Wait for backend to be ready
        if ! wait_for_service "http://localhost:$BACKEND_PORT/api/system/status" "Backend API" 60; then
            echo -e "${RED}❌ Failed to start backend service${NC}"
            exit 1
        fi
    fi
}

# Function to start frontend
start_frontend() {
    echo -e "${BLUE}🔧 Starting frontend service...${NC}"
    
    if check_port $FRONTEND_PORT; then
        echo -e "${YELLOW}⚠️ Frontend port $FRONTEND_PORT is already in use${NC}"
        echo -e "${YELLOW}   Assuming frontend is already running${NC}"
    else
        if [ ! -d "$FRONTEND_DIR" ]; then
            echo -e "${RED}❌ Frontend directory not found: $FRONTEND_DIR${NC}"
            exit 1
        fi
        
        echo -e "${BLUE}   Installing frontend dependencies...${NC}"
        cd $FRONTEND_DIR
        npm install > ../tests/playwright/frontend-install.log 2>&1
        
        echo -e "${BLUE}   Starting frontend development server...${NC}"
        nohup npm run dev > ../tests/playwright/frontend.log 2>&1 &
        FRONTEND_PID=$!
        cd ../tests/playwright/
        
        echo "Frontend PID: $FRONTEND_PID" > frontend.pid
        
        # Wait for frontend to be ready
        if ! wait_for_service "http://localhost:$FRONTEND_PORT" "Frontend" 120; then
            echo -e "${RED}❌ Failed to start frontend service${NC}"
            exit 1
        fi
    fi
}

# Function to install Playwright
install_playwright() {
    echo -e "${BLUE}🔧 Installing Playwright...${NC}"
    
    if [ ! -f "package.json" ]; then
        echo -e "${RED}❌ package.json not found in tests/playwright directory${NC}"
        exit 1
    fi
    
    npm install
    npx playwright install
    npx playwright install-deps
}

# Function to run tests
run_tests() {
    echo -e "${BLUE}🧪 Running API consistency tests...${NC}"
    
    # Create reports directory
    mkdir -p reports
    
    # Run the tests
    echo -e "${BLUE}   Running consistency tests...${NC}"
    npx playwright test api-consistency.spec.js --reporter=html --reporter=json --reporter=junit
    
    echo -e "${BLUE}   Running data validation tests...${NC}"
    npx playwright test api-data-validation.spec.js --reporter=html --reporter=json --reporter=junit
    
    echo -e "${GREEN}✅ All tests completed${NC}"
}

# Function to generate report
generate_report() {
    echo -e "${BLUE}📊 Generating test report...${NC}"
    
    if [ -f "reports/results.json" ]; then
        echo -e "${GREEN}✅ Test results available in reports/results.json${NC}"
    fi
    
    if [ -f "reports/results.xml" ]; then
        echo -e "${GREEN}✅ JUnit results available in reports/results.xml${NC}"
    fi
    
    if [ -d "reports/html" ]; then
        echo -e "${GREEN}✅ HTML report available in reports/html/index.html${NC}"
        echo -e "${BLUE}   To view: npx playwright show-report${NC}"
    fi
}

# Function to cleanup
cleanup() {
    echo -e "${BLUE}🧹 Cleaning up...${NC}"
    
    # Kill backend if we started it
    if [ -f "backend.pid" ]; then
        BACKEND_PID=$(cat backend.pid)
        if kill -0 $BACKEND_PID 2>/dev/null; then
            echo -e "${YELLOW}   Stopping backend (PID: $BACKEND_PID)...${NC}"
            kill $BACKEND_PID
        fi
        rm -f backend.pid
    fi
    
    # Kill frontend if we started it
    if [ -f "frontend.pid" ]; then
        FRONTEND_PID=$(cat frontend.pid)
        if kill -0 $FRONTEND_PID 2>/dev/null; then
            echo -e "${YELLOW}   Stopping frontend (PID: $FRONTEND_PID)...${NC}"
            kill $FRONTEND_PID
        fi
        rm -f frontend.pid
    fi
    
    echo -e "${GREEN}✅ Cleanup completed${NC}"
}

# Trap to ensure cleanup on exit
trap cleanup EXIT

# Main execution
main() {
    echo -e "${BLUE}📋 Test Configuration:${NC}"
    echo "   Backend Port: $BACKEND_PORT"
    echo "   Frontend Port: $FRONTEND_PORT"
    echo "   Test Timeout: $TEST_TIMEOUT seconds"
    echo ""
    
    # Check if we're in the right directory
    if [ ! -f "playwright.config.js" ]; then
        echo -e "${RED}❌ playwright.config.js not found${NC}"
        echo -e "${YELLOW}   Please run this script from the tests/playwright directory${NC}"
        exit 1
    fi
    
    # Install Playwright if needed
    if [ ! -d "node_modules" ]; then
        install_playwright
    fi
    
    # Start services
    start_backend
    start_frontend
    
    # Run tests
    run_tests
    
    # Generate report
    generate_report
    
    echo -e "${GREEN}🎉 API consistency testing completed successfully!${NC}"
}

# Parse command line arguments
case "${1:-}" in
    --help|-h)
        echo "Usage: $0 [options]"
        echo ""
        echo "Options:"
        echo "  --help, -h     Show this help message"
        echo "  --install      Install Playwright dependencies only"
        echo "  --cleanup      Cleanup running services only"
        echo ""
        exit 0
        ;;
    --install)
        install_playwright
        exit 0
        ;;
    --cleanup)
        cleanup
        exit 0
        ;;
    *)
        main
        ;;
esac
