const { chromium } = require('@playwright/test');

async function globalSetup(config) {
  console.log('🚀 Starting global setup for API consistency tests...');
  
  // Launch browser for setup
  const browser = await chromium.launch();
  const page = await browser.newPage();
  
  try {
    // Wait for backend to be ready
    console.log('⏳ Waiting for backend API to be ready...');
    let backendReady = false;
    let attempts = 0;
    const maxAttempts = 30;
    
    while (!backendReady && attempts < maxAttempts) {
      try {
        const response = await page.request.get('http://localhost:8080/api/system/status');
        if (response.ok()) {
          backendReady = true;
          console.log('✅ Backend API is ready');
        }
      } catch (error) {
        attempts++;
        console.log(`⏳ Backend not ready, attempt ${attempts}/${maxAttempts}`);
        await new Promise(resolve => setTimeout(resolve, 2000));
      }
    }
    
    if (!backendReady) {
      throw new Error('Backend API failed to start within timeout period');
    }
    
    // Wait for frontend to be ready
    console.log('⏳ Waiting for frontend to be ready...');
    let frontendReady = false;
    attempts = 0;
    
    while (!frontendReady && attempts < maxAttempts) {
      try {
        await page.goto('http://localhost:3000', { waitUntil: 'networkidle' });
        frontendReady = true;
        console.log('✅ Frontend is ready');
      } catch (error) {
        attempts++;
        console.log(`⏳ Frontend not ready, attempt ${attempts}/${maxAttempts}`);
        await new Promise(resolve => setTimeout(resolve, 2000));
      }
    }
    
    if (!frontendReady) {
      throw new Error('Frontend failed to start within timeout period');
    }
    
    // Setup test data if needed
    console.log('🔧 Setting up test data...');
    
    // Add test cameras if none exist
    try {
      const camerasResponse = await page.request.get('http://localhost:8080/api/cameras');
      const camerasData = await camerasResponse.json();
      
      if (!camerasData.cameras || camerasData.cameras.length === 0) {
        console.log('📹 Adding test cameras...');
        
        const testCameras = [
          {
            id: 'test_camera_01',
            name: 'Test Camera 1',
            url: 'rtsp://admin:sharpi1688@192.168.1.3:554/1/1',
            protocol: 'rtsp',
            username: 'admin',
            password: 'sharpi1688',
            width: 1920,
            height: 1080,
            fps: 25,
            mjpeg_port: 8161,
            enabled: true
          },
          {
            id: 'test_camera_02',
            name: 'Test Camera 2',
            url: 'rtsp://admin:sharpi1688@192.168.1.2:554/1/1',
            protocol: 'rtsp',
            username: 'admin',
            password: 'sharpi1688',
            width: 1920,
            height: 1080,
            fps: 25,
            mjpeg_port: 8162,
            enabled: true
          }
        ];
        
        for (const camera of testCameras) {
          try {
            await page.request.post('http://localhost:8080/api/cameras', {
              data: camera
            });
            console.log(`✅ Added test camera: ${camera.name}`);
          } catch (error) {
            console.log(`⚠️ Failed to add test camera ${camera.name}: ${error.message}`);
          }
        }
      }
    } catch (error) {
      console.log(`⚠️ Failed to setup test cameras: ${error.message}`);
    }
    
    // Verify API endpoints are responding
    console.log('🔍 Verifying API endpoints...');
    
    const endpointsToCheck = [
      '/api/system/status',
      '/api/system/info',
      '/api/cameras',
      '/api/alerts',
      '/api/detection/config'
    ];
    
    for (const endpoint of endpointsToCheck) {
      try {
        const response = await page.request.get(`http://localhost:8080${endpoint}`);
        if (response.ok()) {
          console.log(`✅ ${endpoint} - OK`);
        } else {
          console.log(`⚠️ ${endpoint} - Status: ${response.status()}`);
        }
      } catch (error) {
        console.log(`❌ ${endpoint} - Error: ${error.message}`);
      }
    }
    
    console.log('✅ Global setup completed successfully');
    
  } catch (error) {
    console.error('❌ Global setup failed:', error.message);
    throw error;
  } finally {
    await browser.close();
  }
}

module.exports = globalSetup;
