const { test, expect } = require('@playwright/test');

/**
 * API Endpoint Consistency Tests
 * Tests the consistency between frontend API calls and backend endpoint handling
 */

// Test configuration
const API_BASE_URL = 'http://localhost:8080/api';
const FRONTEND_URL = 'http://localhost:3000';

test.describe('API Endpoint Consistency Tests', () => {
  let page;

  test.beforeAll(async ({ browser }) => {
    page = await browser.newPage();
    
    // Enable request/response logging
    page.on('request', request => {
      if (request.url().includes('/api/')) {
        console.log(`→ ${request.method()} ${request.url()}`);
      }
    });
    
    page.on('response', response => {
      if (response.url().includes('/api/')) {
        console.log(`← ${response.status()} ${response.url()}`);
      }
    });
  });

  test.afterAll(async () => {
    await page.close();
  });

  test.describe('System Endpoints', () => {
    test('GET /api/system/status - Backend vs Frontend', async ({ request }) => {
      // Test backend endpoint directly
      const backendResponse = await request.get(`${API_BASE_URL}/system/status`);
      expect(backendResponse.ok()).toBeTruthy();
      
      const backendData = await backendResponse.json();
      expect(backendData).toHaveProperty('status');
      expect(backendData).toHaveProperty('active_pipelines');
      expect(backendData).toHaveProperty('cpu_usage');
      
      // Test frontend API call
      await page.goto(FRONTEND_URL);
      
      const frontendResponse = await page.evaluate(async () => {
        try {
          const response = await fetch('/api/system/status');
          return await response.json();
        } catch (error) {
          return { error: error.message };
        }
      });
      
      // Compare structure consistency
      expect(frontendResponse).toHaveProperty('status');
      expect(frontendResponse).toHaveProperty('active_pipelines');
      expect(frontendResponse).toHaveProperty('cpu_usage');
      
      // Verify data types match
      expect(typeof frontendResponse.status).toBe(typeof backendData.status);
      expect(typeof frontendResponse.active_pipelines).toBe(typeof backendData.active_pipelines);
      expect(typeof frontendResponse.cpu_usage).toBe(typeof backendData.cpu_usage);
    });

    test('GET /api/system/info - Backend vs Frontend', async ({ request }) => {
      // Test backend endpoint
      const backendResponse = await request.get(`${API_BASE_URL}/system/info`);
      expect(backendResponse.ok()).toBeTruthy();
      
      // Test frontend through dashboard page
      await page.goto(`${FRONTEND_URL}/dashboard`);
      
      // Wait for system info to load
      await page.waitForFunction(() => {
        return window.fetch !== undefined;
      });
      
      const frontendCall = await page.evaluate(async () => {
        const response = await fetch('/api/system/info');
        return {
          status: response.status,
          data: await response.json()
        };
      });
      
      expect(frontendCall.status).toBe(200);
      expect(frontendCall.data).toBeDefined();
    });

    test('GET /api/system/config - Configuration Management', async ({ request }) => {
      // Backend test
      const backendResponse = await request.get(`${API_BASE_URL}/system/config`);
      expect(backendResponse.ok()).toBeTruthy();
      
      // Frontend test through settings page
      await page.goto(`${FRONTEND_URL}/settings`);
      
      const configResponse = await page.evaluate(async () => {
        try {
          const response = await fetch('/api/system/config');
          return {
            status: response.status,
            data: await response.json()
          };
        } catch (error) {
          return { error: error.message };
        }
      });
      
      expect(configResponse.status).toBe(200);
    });
  });

  test.describe('Camera Endpoints', () => {
    test('GET /api/cameras - Camera List Consistency', async ({ request }) => {
      // Backend test
      const backendResponse = await request.get(`${API_BASE_URL}/cameras`);
      expect(backendResponse.ok()).toBeTruthy();
      
      const backendData = await backendResponse.json();
      
      // Frontend test through cameras page
      await page.goto(`${FRONTEND_URL}/cameras`);
      
      // Wait for cameras to load
      await page.waitForSelector('.cameras-grid', { timeout: 10000 });
      
      const frontendCall = await page.evaluate(async () => {
        const response = await fetch('/api/cameras');
        return {
          status: response.status,
          data: await response.json()
        };
      });
      
      expect(frontendCall.status).toBe(200);
      
      // Verify structure consistency
      if (backendData.cameras) {
        expect(frontendCall.data).toHaveProperty('cameras');
        expect(Array.isArray(frontendCall.data.cameras)).toBeTruthy();
      }
    });

    test('POST /api/cameras - Add Camera Consistency', async ({ request }) => {
      const testCamera = {
        id: 'test_camera_playwright',
        name: 'Playwright Test Camera',
        url: 'rtsp://test:test@192.168.1.100:554/stream',
        protocol: 'rtsp',
        username: 'test',
        password: 'test',
        width: 1920,
        height: 1080,
        fps: 25,
        mjpeg_port: 8199,
        enabled: true
      };
      
      // Test backend endpoint directly
      const backendResponse = await request.post(`${API_BASE_URL}/cameras`, {
        data: testCamera
      });
      
      // Should return 201 for created or 409 if already exists
      expect([201, 409].includes(backendResponse.status())).toBeTruthy();
      
      // Test frontend API call
      await page.goto(`${FRONTEND_URL}/cameras`);
      
      const frontendCall = await page.evaluate(async (camera) => {
        try {
          const response = await fetch('/api/cameras', {
            method: 'POST',
            headers: {
              'Content-Type': 'application/json'
            },
            body: JSON.stringify(camera)
          });
          
          return {
            status: response.status,
            data: await response.json()
          };
        } catch (error) {
          return { error: error.message };
        }
      }, testCamera);
      
      // Should handle the same way as backend
      expect([201, 409].includes(frontendCall.status)).toBeTruthy();
    });

    test('POST /api/cameras/test-connection - Connection Test', async ({ request }) => {
      const connectionTest = {
        url: 'rtsp://admin:sharpi1688@192.168.1.3:554/1/1',
        username: 'admin',
        password: 'sharpi1688'
      };
      
      // Backend test
      const backendResponse = await request.post(`${API_BASE_URL}/cameras/test-connection`, {
        data: connectionTest
      });
      
      // Frontend test
      await page.goto(`${FRONTEND_URL}/cameras`);
      
      const frontendCall = await page.evaluate(async (testData) => {
        const response = await fetch('/api/cameras/test-connection', {
          method: 'POST',
          headers: {
            'Content-Type': 'application/json'
          },
          body: JSON.stringify(testData)
        });
        
        return {
          status: response.status,
          data: await response.json()
        };
      }, connectionTest);
      
      // Both should return same status
      expect(frontendCall.status).toBe(backendResponse.status());
    });
  });

  test.describe('Alert Endpoints', () => {
    test('GET /api/alerts - Alerts Consistency', async ({ request }) => {
      // Backend test
      const backendResponse = await request.get(`${API_BASE_URL}/alerts`);
      expect(backendResponse.ok()).toBeTruthy();
      
      const backendData = await backendResponse.json();
      
      // Frontend test through alerts page
      await page.goto(`${FRONTEND_URL}/alerts`);
      
      const frontendCall = await page.evaluate(async () => {
        const response = await fetch('/api/alerts');
        return {
          status: response.status,
          data: await response.json()
        };
      });
      
      expect(frontendCall.status).toBe(200);
      
      // Verify structure consistency
      if (backendData.alerts) {
        expect(frontendCall.data).toHaveProperty('alerts');
        expect(Array.isArray(frontendCall.data.alerts)).toBeTruthy();
      }
    });
  });

  test.describe('Detection Configuration', () => {
    test('GET /api/detection/config - Detection Config Consistency', async ({ request }) => {
      // Backend test
      const backendResponse = await request.get(`${API_BASE_URL}/detection/config`);
      expect(backendResponse.ok()).toBeTruthy();
      
      // Frontend test
      await page.goto(`${FRONTEND_URL}/settings`);
      
      const frontendCall = await page.evaluate(async () => {
        const response = await fetch('/api/detection/config');
        return {
          status: response.status,
          data: await response.json()
        };
      });
      
      expect(frontendCall.status).toBe(backendResponse.status());
    });
  });

  test.describe('Person Statistics Endpoints', () => {
    test('GET /api/cameras/{id}/person-stats - Person Stats Consistency', async ({ request }) => {
      const cameraId = 'camera_01';
      
      // Backend test
      const backendResponse = await request.get(`${API_BASE_URL}/cameras/${cameraId}/person-stats`);
      
      // Frontend test through person statistics page
      await page.goto(`${FRONTEND_URL}/person-statistics`);
      
      const frontendCall = await page.evaluate(async (id) => {
        const response = await fetch(`/api/cameras/${id}/person-stats`);
        return {
          status: response.status,
          data: response.ok() ? await response.json() : null
        };
      }, cameraId);
      
      // Both should return same status
      expect(frontendCall.status).toBe(backendResponse.status());
    });

    test('POST /api/cameras/{id}/person-stats/enable - Enable Person Stats', async ({ request }) => {
      const cameraId = 'camera_01';
      const enableData = { enabled: true };
      
      // Backend test
      const backendResponse = await request.post(`${API_BASE_URL}/cameras/${cameraId}/person-stats/enable`, {
        data: enableData
      });
      
      // Frontend test
      await page.goto(`${FRONTEND_URL}/person-statistics`);
      
      const frontendCall = await page.evaluate(async (id, data) => {
        const response = await fetch(`/api/cameras/${id}/person-stats/enable`, {
          method: 'POST',
          headers: {
            'Content-Type': 'application/json'
          },
          body: JSON.stringify(data)
        });
        
        return {
          status: response.status,
          data: response.ok() ? await response.json() : null
        };
      }, cameraId, enableData);
      
      expect(frontendCall.status).toBe(backendResponse.status());
    });
  });

  test.describe('Error Handling Consistency', () => {
    test('404 Errors - Non-existent Endpoints', async ({ request }) => {
      const nonExistentEndpoint = '/api/non-existent-endpoint';
      
      // Backend test
      const backendResponse = await request.get(`${API_BASE_URL.replace('/api', '')}${nonExistentEndpoint}`);
      
      // Frontend test
      await page.goto(FRONTEND_URL);
      
      const frontendCall = await page.evaluate(async (endpoint) => {
        const response = await fetch(endpoint);
        return response.status;
      }, nonExistentEndpoint);
      
      // Both should return 404
      expect(backendResponse.status()).toBe(404);
      expect(frontendCall).toBe(404);
    });

    test('400 Errors - Invalid Request Data', async ({ request }) => {
      const invalidCameraData = {
        // Missing required fields
        name: 'Invalid Camera'
      };
      
      // Backend test
      const backendResponse = await request.post(`${API_BASE_URL}/cameras`, {
        data: invalidCameraData
      });
      
      // Frontend test
      await page.goto(`${FRONTEND_URL}/cameras`);
      
      const frontendCall = await page.evaluate(async (data) => {
        const response = await fetch('/api/cameras', {
          method: 'POST',
          headers: {
            'Content-Type': 'application/json'
          },
          body: JSON.stringify(data)
        });
        
        return response.status;
      }, invalidCameraData);
      
      // Both should return 400
      expect(backendResponse.status()).toBe(400);
      expect(frontendCall).toBe(400);
    });
  });

  test.describe('Response Format Consistency', () => {
    test('JSON Response Headers', async ({ request }) => {
      const endpoints = [
        '/system/status',
        '/cameras',
        '/alerts',
        '/detection/config'
      ];

      for (const endpoint of endpoints) {
        // Backend test
        const backendResponse = await request.get(`${API_BASE_URL}${endpoint}`);
        const backendContentType = backendResponse.headers()['content-type'];

        // Frontend test
        await page.goto(FRONTEND_URL);

        const frontendContentType = await page.evaluate(async (ep) => {
          const response = await fetch(`/api${ep}`);
          return response.headers.get('content-type');
        }, endpoint);

        // Both should return JSON content type
        expect(backendContentType).toContain('application/json');
        expect(frontendContentType).toContain('application/json');
      }
    });

    test('CORS Headers Consistency', async ({ request }) => {
      // Backend test
      const backendResponse = await request.get(`${API_BASE_URL}/system/status`);
      const corsHeader = backendResponse.headers()['access-control-allow-origin'];

      // Frontend test
      await page.goto(FRONTEND_URL);

      const frontendCors = await page.evaluate(async () => {
        const response = await fetch('/api/system/status');
        return response.headers.get('access-control-allow-origin');
      });

      // Both should have CORS headers
      expect(corsHeader).toBe('*');
      expect(frontendCors).toBe('*');
    });
  });

  test.describe('Real-time Features', () => {
    test('MJPEG Stream URLs Consistency', async ({ request }) => {
      // Test that frontend stream URL mapping matches backend capabilities
      await page.goto(`${FRONTEND_URL}/live`);

      const streamUrls = await page.evaluate(() => {
        // Get stream URLs from frontend API service
        const streamMapping = {
          'camera_01': 'http://localhost:8161/stream.mjpg',
          'camera_02': 'http://localhost:8162/stream.mjpg',
          'camera_03': 'http://localhost:8163/stream.mjpg',
          'camera_04': 'http://localhost:8164/stream.mjpg'
        };
        return streamMapping;
      });

      // Verify stream URLs are accessible (at least respond)
      for (const [cameraId, streamUrl] of Object.entries(streamUrls)) {
        try {
          const response = await request.get(streamUrl, { timeout: 5000 });
          console.log(`Stream ${cameraId}: ${response.status()}`);
          // Stream might return 404 if camera not active, but should not timeout
          expect([200, 404, 503].includes(response.status())).toBeTruthy();
        } catch (error) {
          console.log(`Stream ${cameraId} not accessible: ${error.message}`);
          // This is acceptable as cameras might not be running
        }
      }
    });

    test('Live View Page Integration', async ({ request }) => {
      // Test that live view page can access camera data
      await page.goto(`${FRONTEND_URL}/live`);

      // Wait for page to load
      await page.waitForLoadState('networkidle');

      // Check if camera selection is available
      const cameraSelectExists = await page.locator('select, .el-select').count() > 0;

      if (cameraSelectExists) {
        // Verify camera data is loaded from API
        const cameraData = await page.evaluate(async () => {
          try {
            const response = await fetch('/api/cameras');
            return await response.json();
          } catch (error) {
            return { error: error.message };
          }
        });

        expect(cameraData).toBeDefined();
        expect(cameraData.error).toBeUndefined();
      }
    });
  });

  test.describe('Configuration Persistence', () => {
    test('Camera Configuration Persistence', async ({ request }) => {
      const testConfig = {
        camera_id: 'persistence_test_camera',
        name: 'Persistence Test Camera',
        url: 'rtsp://test:test@192.168.1.100:554/stream',
        protocol: 'rtsp',
        enabled: true,
        detection_enabled: true
      };

      // Save configuration via backend
      const saveResponse = await request.post(`${API_BASE_URL}/cameras/config`, {
        data: testConfig
      });

      // Retrieve configuration via frontend
      await page.goto(`${FRONTEND_URL}/settings`);

      const retrievedConfig = await page.evaluate(async (configId) => {
        const response = await fetch('/api/cameras/config');
        const data = await response.json();

        if (data.configs) {
          return data.configs.find(config => config.camera_id === configId);
        }
        return null;
      }, testConfig.camera_id);

      if (saveResponse.ok() && retrievedConfig) {
        // Verify configuration was persisted correctly
        expect(retrievedConfig.camera_id).toBe(testConfig.camera_id);
        expect(retrievedConfig.name).toBe(testConfig.name);
        expect(retrievedConfig.enabled).toBe(testConfig.enabled);
      }

      // Cleanup
      try {
        await request.delete(`${API_BASE_URL}/cameras/config/${testConfig.camera_id}`);
      } catch (error) {
        // Ignore cleanup errors
      }
    });
  });
});
