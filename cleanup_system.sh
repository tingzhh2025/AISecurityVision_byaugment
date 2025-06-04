#!/bin/bash

# AI Security Vision System - Complete Cleanup Script
# This script ensures both frontend and backend start with a completely clean state

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Function to print colored output
print_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

print_header() {
    echo -e "\n${BLUE}=== $1 ===${NC}"
}

# Check if running as root for some operations
check_permissions() {
    if [[ $EUID -eq 0 ]]; then
        print_warning "Running as root. Some cleanup operations may affect system-wide settings."
        read -p "Continue? (y/N): " -n 1 -r
        echo
        if [[ ! $REPLY =~ ^[Yy]$ ]]; then
            exit 1
        fi
    fi
}

# Stop running processes
stop_processes() {
    print_header "Stopping Running Processes"
    
    # Stop C++ backend if running
    print_info "Stopping C++ backend processes..."
    pkill -f "AISecurityVision" 2>/dev/null || true
    pkill -f "aibox" 2>/dev/null || true
    
    # Stop frontend development server if running
    print_info "Stopping frontend development server..."
    pkill -f "vite" 2>/dev/null || true
    pkill -f "npm.*dev" 2>/dev/null || true
    pkill -f "yarn.*dev" 2>/dev/null || true
    
    # Wait a moment for processes to stop
    sleep 2
    
    print_success "Processes stopped"
}

# Clean backend data
clean_backend() {
    print_header "Cleaning Backend Data"
    
    # Remove database file
    if [ -f "aibox.db" ]; then
        print_info "Removing database file: aibox.db"
        rm -f aibox.db
        print_success "Database file removed"
    else
        print_info "Database file not found (already clean)"
    fi
    
    # Remove any log files
    print_info "Removing log files..."
    rm -f *.log 2>/dev/null || true
    rm -f logs/*.log 2>/dev/null || true
    
    # Remove any temporary files
    print_info "Removing temporary files..."
    rm -f /tmp/aibox_* 2>/dev/null || true
    
    print_success "Backend data cleaned"
}

# Clean frontend data
clean_frontend() {
    print_header "Cleaning Frontend Data"
    
    if [ -d "web-ui" ]; then
        cd web-ui
        
        # Remove node_modules and package-lock.json for a fresh install
        print_info "Removing node_modules and lock files..."
        rm -rf node_modules 2>/dev/null || true
        rm -f package-lock.json 2>/dev/null || true
        rm -f yarn.lock 2>/dev/null || true
        rm -f pnpm-lock.yaml 2>/dev/null || true
        
        # Remove build artifacts
        print_info "Removing build artifacts..."
        rm -rf dist 2>/dev/null || true
        rm -rf .vite 2>/dev/null || true
        rm -rf .cache 2>/dev/null || true
        
        # Remove environment files (user will need to recreate)
        print_info "Removing environment configuration files..."
        rm -f .env.local 2>/dev/null || true
        rm -f .env.development 2>/dev/null || true
        rm -f .env.production 2>/dev/null || true
        
        cd ..
        print_success "Frontend data cleaned"
    else
        print_warning "Frontend directory (web-ui) not found"
    fi
}

# Clean browser data (instructions)
clean_browser_instructions() {
    print_header "Browser Cache Cleanup Instructions"
    
    print_info "To ensure a completely clean frontend state, please:"
    echo "  1. Open your browser's Developer Tools (F12)"
    echo "  2. Right-click on the refresh button"
    echo "  3. Select 'Empty Cache and Hard Reload'"
    echo "  4. Or use Ctrl+Shift+R (Chrome/Firefox) or Cmd+Shift+R (Safari)"
    echo ""
    print_info "Alternatively, you can:"
    echo "  1. Open browser settings"
    echo "  2. Go to Privacy/Security settings"
    echo "  3. Clear browsing data for the last hour"
    echo "  4. Make sure to include 'Cached images and files'"
    echo ""
    print_warning "This step is important to remove any cached camera data from the browser!"
}

# Rebuild and restart instructions
rebuild_instructions() {
    print_header "Rebuild and Restart Instructions"
    
    print_info "To start with a clean system:"
    echo ""
    echo "1. Backend (C++):"
    echo "   cd /path/to/your/project"
    echo "   mkdir -p build && cd build"
    echo "   cmake .."
    echo "   make -j\$(nproc)"
    echo "   ./AISecurityVision"
    echo ""
    echo "2. Frontend (Vue3):"
    echo "   cd web-ui"
    echo "   npm install  # or yarn install"
    echo "   cp .env.example .env.local"
    echo "   # Edit .env.local with your settings"
    echo "   npm run dev  # or yarn dev"
    echo ""
    print_success "System is now ready for a clean start!"
}

# Main cleanup function
main() {
    print_header "AI Security Vision System - Complete Cleanup"
    print_info "This script will remove all data and ensure a clean system state."
    print_warning "This action cannot be undone!"
    echo ""
    
    read -p "Are you sure you want to proceed? (y/N): " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        print_info "Cleanup cancelled."
        exit 0
    fi
    
    check_permissions
    stop_processes
    clean_backend
    clean_frontend
    clean_browser_instructions
    rebuild_instructions
    
    print_header "Cleanup Complete"
    print_success "System has been completely cleaned!"
    print_info "You can now restart the system with a clean state."
}

# Run main function
main "$@"
