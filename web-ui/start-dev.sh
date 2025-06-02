#!/bin/bash

# AI Security Vision Web UI Development Server Startup Script
# This script starts the frontend development server with proxy bypass

echo "🚀 Starting AI Security Vision Web UI Development Server..."
echo "📍 Working directory: $(pwd)"
echo "🌐 Network proxy bypass: 127.0.0.1,localhost"

# Set environment variables to bypass proxy for local connections
export NO_PROXY=127.0.0.1,localhost
export no_proxy=127.0.0.1,localhost

# Start the development server
echo "🔧 Starting Vite development server..."
npm run dev -- --host 0.0.0.0 --port 3000

echo "✅ Development server started successfully!"
echo "🌐 Local:   http://localhost:3000/"
echo "🌐 Network: http://$(hostname -I | awk '{print $1}'):3000/"
