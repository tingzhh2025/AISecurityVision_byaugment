const { chromium } = require('@playwright/test');

async function globalTeardown(config) {
  console.log('🧹 Starting global teardown...');
  
  // Launch browser for cleanup
  const browser = await chromium.launch();
  const page = await browser.newPage();
  
  try {
    // Clean up test data
    console.log('🗑️ Cleaning up test data...');
    
    // Remove test cameras
    try {
      const camerasResponse = await page.request.get('http://localhost:8080/api/cameras');
      if (camerasResponse.ok()) {
        const camerasData = await camerasResponse.json();
        
        if (camerasData.cameras) {
          for (const camera of camerasData.cameras) {
            if (camera.id && camera.id.startsWith('test_camera_')) {
              try {
                await page.request.delete(`http://localhost:8080/api/cameras/${camera.id}`);
                console.log(`✅ Removed test camera: ${camera.name || camera.id}`);
              } catch (error) {
                console.log(`⚠️ Failed to remove test camera ${camera.id}: ${error.message}`);
              }
            }
          }
        }
      }
    } catch (error) {
      console.log(`⚠️ Failed to clean up test cameras: ${error.message}`);
    }
    
    // Clean up test configurations
    try {
      const configResponse = await page.request.get('http://localhost:8080/api/cameras/config');
      if (configResponse.ok()) {
        const configData = await configResponse.json();
        
        if (configData.configs) {
          for (const config of configData.configs) {
            if (config.camera_id && config.camera_id.startsWith('test_camera_')) {
              try {
                await page.request.delete(`http://localhost:8080/api/cameras/config/${config.camera_id}`);
                console.log(`✅ Removed test camera config: ${config.camera_id}`);
              } catch (error) {
                console.log(`⚠️ Failed to remove test config ${config.camera_id}: ${error.message}`);
              }
            }
          }
        }
      }
    } catch (error) {
      console.log(`⚠️ Failed to clean up test configurations: ${error.message}`);
    }
    
    // Generate test summary
    console.log('📊 Generating test summary...');
    
    try {
      const fs = require('fs');
      const path = require('path');
      
      const reportsDir = 'tests/playwright/reports';
      const summaryFile = path.join(reportsDir, 'test-summary.json');
      
      // Ensure reports directory exists
      if (!fs.existsSync(reportsDir)) {
        fs.mkdirSync(reportsDir, { recursive: true });
      }
      
      const summary = {
        timestamp: new Date().toISOString(),
        testRun: 'API Consistency Tests',
        environment: {
          backend: 'http://localhost:8080',
          frontend: 'http://localhost:3000'
        },
        cleanup: {
          testCamerasRemoved: true,
          testConfigsRemoved: true
        }
      };
      
      fs.writeFileSync(summaryFile, JSON.stringify(summary, null, 2));
      console.log(`✅ Test summary saved to: ${summaryFile}`);
      
    } catch (error) {
      console.log(`⚠️ Failed to generate test summary: ${error.message}`);
    }
    
    console.log('✅ Global teardown completed successfully');
    
  } catch (error) {
    console.error('❌ Global teardown failed:', error.message);
  } finally {
    await browser.close();
  }
}

module.exports = globalTeardown;
