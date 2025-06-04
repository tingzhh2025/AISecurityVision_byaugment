# AI Security Vision System - Clean State Guide

This guide explains how to ensure both the frontend and backend start with a completely clean state, with no mock data, cached configurations, or simulated camera entries.

## Quick Cleanup (Automated)

Run the automated cleanup script:

```bash
./cleanup_system.sh
```

This script will:
- Stop all running processes
- Remove the database file (`aibox.db`)
- Clean frontend build artifacts and dependencies
- Provide instructions for browser cache cleanup

## Manual Cleanup Steps

### 1. Backend Cleanup

```bash
# Stop the C++ application
pkill -f AISecurityVision

# Remove the database file
rm -f aibox.db

# Remove any log files
rm -f *.log
```

### 2. Frontend Cleanup

```bash
cd web-ui

# Remove node modules and lock files
rm -rf node_modules
rm -f package-lock.json yarn.lock pnpm-lock.yaml

# Remove build artifacts
rm -rf dist .vite .cache

# Remove environment files (optional)
rm -f .env.local .env.development .env.production
```

### 3. Browser Cache Cleanup

**Important**: The browser may cache camera data and API responses.

#### Method 1: Hard Refresh
- Open Developer Tools (F12)
- Right-click the refresh button
- Select "Empty Cache and Hard Reload"

#### Method 2: Clear Browser Data
- Open browser settings
- Go to Privacy/Security
- Clear browsing data for "Last hour"
- Include "Cached images and files"

#### Method 3: Incognito/Private Mode
- Open the application in incognito/private browsing mode
- This ensures no cached data is used

## Starting Clean

### 1. Backend

```bash
# Build the application
mkdir -p build && cd build
cmake ..
make -j$(nproc)

# Start with clean database
./AISecurityVision
```

### 2. Frontend

```bash
cd web-ui

# Install dependencies
npm install  # or yarn install

# Create environment file
cp .env.example .env.local

# Edit .env.local with your backend URL
# VITE_API_BASE_URL=http://localhost:8080

# Start development server
npm run dev  # or yarn dev
```

## Verification

After cleanup and restart:

1. **Backend**: Check that no cameras are loaded:
   ```bash
   curl http://localhost:8080/api/cameras
   # Should return: {"cameras":[],"total":0,"timestamp":"..."}
   ```

2. **Frontend**: Open the web interface:
   - Cameras page should show "No cameras configured"
   - Live view should show empty grid
   - No mock or simulated data should appear

## What Was Removed

### Frontend Mock Data Removed:
- ✅ Hardcoded 8-camera fallback data in `web-ui/src/stores/system.js`
- ✅ Status override logic that forced cameras to appear "online"
- ✅ Hardcoded stream URLs in `web-ui/src/views/Live.vue`
- ✅ Browser localStorage/sessionStorage cache

### Backend Mock Data Removed:
- ✅ In-memory camera configurations (`m_cameraConfigs`)
- ✅ Database file with any existing camera data
- ✅ Hardcoded camera fallback logic in `src/main.cpp`

### System Improvements:
- ✅ Added `clearInMemoryConfigurations()` method to APIService
- ✅ Removed `testMode` and `useRealCameras` parameters
- ✅ Made system fully database-driven
- ✅ Clean state initialization on startup

## Adding Cameras

After cleanup, add cameras through the web interface:

1. Open the web interface (usually http://localhost:3000)
2. Go to "Cameras" page
3. Click "Add Camera"
4. Fill in camera details:
   - Name
   - RTSP URL
   - Username/Password
   - Resolution and FPS
5. Save the camera

The camera will be stored in the database and synchronized between frontend and backend.

## Troubleshooting

### Frontend Still Shows Cameras
- Clear browser cache completely
- Try incognito/private mode
- Check browser Developer Tools → Application → Storage

### Backend Still Returns Cameras
- Ensure `aibox.db` file is deleted
- Restart the C++ application
- Check that `clearInMemoryConfigurations()` is called on startup

### API Synchronization Issues
- Restart both frontend and backend
- Check network connectivity between components
- Verify API endpoints are accessible

## Configuration Storage

After cleanup, all configuration is stored in:
- **Database**: `aibox.db` (SQLite)
  - Camera configurations
  - System settings
  - Detection parameters
- **Frontend**: No persistent storage (loads from backend)

This ensures complete synchronization between frontend and backend components.
