const { test, expect } = require('@playwright/test');

/**
 * API Data Validation Tests
 * Tests the data structure and validation consistency between frontend and backend
 */

const API_BASE_URL = 'http://localhost:8080/api';
const FRONTEND_URL = 'http://localhost:3000';

test.describe('API Data Validation Tests', () => {
  test.describe('Camera Data Validation', () => {
    test('Camera object structure consistency', async ({ request, page }) => {
      // Get camera data from backend
      const backendResponse = await request.get(`${API_BASE_URL}/cameras`);
      expect(backendResponse.ok()).toBeTruthy();
      
      const backendData = await backendResponse.json();
      
      // Get camera data from frontend
      await page.goto(FRONTEND_URL);
      
      const frontendData = await page.evaluate(async () => {
        const response = await fetch('/api/cameras');
        return await response.json();
      });
      
      // Validate structure consistency
      if (backendData.cameras && backendData.cameras.length > 0) {
        const backendCamera = backendData.cameras[0];
        
        if (frontendData.cameras && frontendData.cameras.length > 0) {
          const frontendCamera = frontendData.cameras[0];
          
          // Check required fields exist in both
          const requiredFields = ['id', 'name', 'url', 'status'];
          
          for (const field of requiredFields) {
            expect(backendCamera).toHaveProperty(field);
            expect(frontendCamera).toHaveProperty(field);
            
            // Check data types match
            expect(typeof backendCamera[field]).toBe(typeof frontendCamera[field]);
          }
        }
      }
    });

    test('Camera creation data validation', async ({ request, page }) => {
      const validCameraData = {
        id: 'validation_test_camera',
        name: 'Validation Test Camera',
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
      
      // Test backend validation
      const backendResponse = await request.post(`${API_BASE_URL}/cameras`, {
        data: validCameraData
      });
      
      // Test frontend validation
      await page.goto(`${FRONTEND_URL}/cameras`);
      
      const frontendResponse = await page.evaluate(async (data) => {
        const response = await fetch('/api/cameras', {
          method: 'POST',
          headers: {
            'Content-Type': 'application/json'
          },
          body: JSON.stringify(data)
        });
        
        return {
          status: response.status,
          data: await response.json()
        };
      }, validCameraData);
      
      // Both should accept valid data
      expect([200, 201, 409].includes(backendResponse.status())).toBeTruthy();
      expect([200, 201, 409].includes(frontendResponse.status)).toBeTruthy();
      
      // Clean up
      try {
        await request.delete(`${API_BASE_URL}/cameras/${validCameraData.id}`);
      } catch (error) {
        // Ignore cleanup errors
      }
    });

    test('Camera data field validation', async ({ request, page }) => {
      const invalidDataTests = [
        {
          name: 'Missing required ID',
          data: {
            name: 'Test Camera',
            url: 'rtsp://test:test@192.168.1.100:554/stream',
            protocol: 'rtsp'
          },
          expectedStatus: 400
        },
        {
          name: 'Missing required URL',
          data: {
            id: 'test_camera_no_url',
            name: 'Test Camera',
            protocol: 'rtsp'
          },
          expectedStatus: 400
        },
        {
          name: 'Invalid protocol',
          data: {
            id: 'test_camera_invalid_protocol',
            name: 'Test Camera',
            url: 'rtsp://test:test@192.168.1.100:554/stream',
            protocol: 'invalid_protocol'
          },
          expectedStatus: 400
        }
      ];
      
      for (const testCase of invalidDataTests) {
        // Test backend validation
        const backendResponse = await request.post(`${API_BASE_URL}/cameras`, {
          data: testCase.data
        });
        
        // Test frontend validation
        await page.goto(`${FRONTEND_URL}/cameras`);
        
        const frontendResponse = await page.evaluate(async (data) => {
          const response = await fetch('/api/cameras', {
            method: 'POST',
            headers: {
              'Content-Type': 'application/json'
            },
            body: JSON.stringify(data)
          });
          
          return response.status;
        }, testCase.data);
        
        // Both should reject invalid data with same status
        expect(backendResponse.status()).toBe(testCase.expectedStatus);
        expect(frontendResponse).toBe(testCase.expectedStatus);
        
        console.log(`✅ ${testCase.name}: Backend ${backendResponse.status()}, Frontend ${frontendResponse}`);
      }
    });
  });

  test.describe('System Status Data Validation', () => {
    test('System status response structure', async ({ request, page }) => {
      // Backend test
      const backendResponse = await request.get(`${API_BASE_URL}/system/status`);
      expect(backendResponse.ok()).toBeTruthy();
      
      const backendData = await backendResponse.json();
      
      // Frontend test
      await page.goto(FRONTEND_URL);
      
      const frontendData = await page.evaluate(async () => {
        const response = await fetch('/api/system/status');
        return await response.json();
      });
      
      // Validate required fields
      const requiredFields = ['status', 'active_pipelines', 'cpu_usage'];
      
      for (const field of requiredFields) {
        expect(backendData).toHaveProperty(field);
        expect(frontendData).toHaveProperty(field);
        
        // Validate data types
        if (field === 'status') {
          expect(typeof backendData[field]).toBe('string');
          expect(typeof frontendData[field]).toBe('string');
        } else {
          expect(typeof backendData[field]).toBe('number');
          expect(typeof frontendData[field]).toBe('number');
        }
      }
      
      // Validate value ranges
      expect(backendData.cpu_usage).toBeGreaterThanOrEqual(0);
      expect(backendData.cpu_usage).toBeLessThanOrEqual(100);
      expect(frontendData.cpu_usage).toBeGreaterThanOrEqual(0);
      expect(frontendData.cpu_usage).toBeLessThanOrEqual(100);
      
      expect(backendData.active_pipelines).toBeGreaterThanOrEqual(0);
      expect(frontendData.active_pipelines).toBeGreaterThanOrEqual(0);
    });
  });

  test.describe('Alert Data Validation', () => {
    test('Alert object structure consistency', async ({ request, page }) => {
      // Backend test
      const backendResponse = await request.get(`${API_BASE_URL}/alerts`);
      expect(backendResponse.ok()).toBeTruthy();
      
      const backendData = await backendResponse.json();
      
      // Frontend test
      await page.goto(`${FRONTEND_URL}/alerts`);
      
      const frontendData = await page.evaluate(async () => {
        const response = await fetch('/api/alerts');
        return await response.json();
      });
      
      // Validate structure
      expect(backendData).toHaveProperty('alerts');
      expect(frontendData).toHaveProperty('alerts');
      
      expect(Array.isArray(backendData.alerts)).toBeTruthy();
      expect(Array.isArray(frontendData.alerts)).toBeTruthy();
      
      // If alerts exist, validate structure
      if (backendData.alerts.length > 0 && frontendData.alerts.length > 0) {
        const backendAlert = backendData.alerts[0];
        const frontendAlert = frontendData.alerts[0];
        
        const expectedFields = ['id', 'title', 'level', 'timestamp'];
        
        for (const field of expectedFields) {
          if (backendAlert.hasOwnProperty(field)) {
            expect(frontendAlert).toHaveProperty(field);
            expect(typeof backendAlert[field]).toBe(typeof frontendAlert[field]);
          }
        }
      }
    });
  });

  test.describe('Person Statistics Data Validation', () => {
    test('Person stats response structure', async ({ request, page }) => {
      const cameraId = 'camera_01';
      
      // Backend test
      const backendResponse = await request.get(`${API_BASE_URL}/cameras/${cameraId}/person-stats`);
      
      // Frontend test
      await page.goto(`${FRONTEND_URL}/person-statistics`);
      
      const frontendData = await page.evaluate(async (id) => {
        const response = await fetch(`/api/cameras/${id}/person-stats`);
        return {
          status: response.status,
          data: response.ok() ? await response.json() : null
        };
      }, cameraId);
      
      // Both should return same status
      expect(frontendData.status).toBe(backendResponse.status());
      
      // If successful, validate structure
      if (backendResponse.ok() && frontendData.data) {
        const backendData = await backendResponse.json();
        
        // Validate common structure
        const expectedFields = ['camera_id', 'enabled'];
        
        for (const field of expectedFields) {
          if (backendData.hasOwnProperty(field)) {
            expect(frontendData.data).toHaveProperty(field);
            expect(typeof backendData[field]).toBe(typeof frontendData.data[field]);
          }
        }
      }
    });

    test('Person stats configuration validation', async ({ request, page }) => {
      const cameraId = 'camera_01';
      const configData = {
        enabled: true,
        age_detection: true,
        gender_detection: true,
        confidence_threshold: 0.7
      };
      
      // Backend test
      const backendResponse = await request.post(`${API_BASE_URL}/cameras/${cameraId}/person-stats/config`, {
        data: configData
      });
      
      // Frontend test
      await page.goto(`${FRONTEND_URL}/person-statistics`);
      
      const frontendResponse = await page.evaluate(async (id, config) => {
        const response = await fetch(`/api/cameras/${id}/person-stats/config`, {
          method: 'POST',
          headers: {
            'Content-Type': 'application/json'
          },
          body: JSON.stringify(config)
        });
        
        return {
          status: response.status,
          data: response.ok() ? await response.json() : null
        };
      }, cameraId, configData);
      
      // Both should handle configuration the same way
      expect(frontendResponse.status).toBe(backendResponse.status());
    });
  });

  test.describe('Error Response Validation', () => {
    test('Error response format consistency', async ({ request, page }) => {
      const invalidEndpoint = '/api/invalid-endpoint';
      
      // Backend test
      const backendResponse = await request.get(`http://localhost:8080${invalidEndpoint}`);
      
      // Frontend test
      await page.goto(FRONTEND_URL);
      
      const frontendResponse = await page.evaluate(async (endpoint) => {
        const response = await fetch(endpoint);
        return {
          status: response.status,
          data: response.ok() ? await response.json() : null
        };
      }, invalidEndpoint);
      
      // Both should return 404
      expect(backendResponse.status()).toBe(404);
      expect(frontendResponse.status).toBe(404);
    });

    test('Validation error format consistency', async ({ request, page }) => {
      const invalidData = {
        // Missing required fields
        name: 'Invalid Camera'
      };
      
      // Backend test
      const backendResponse = await request.post(`${API_BASE_URL}/cameras`, {
        data: invalidData
      });
      
      // Frontend test
      await page.goto(`${FRONTEND_URL}/cameras`);
      
      const frontendResponse = await page.evaluate(async (data) => {
        const response = await fetch('/api/cameras', {
          method: 'POST',
          headers: {
            'Content-Type': 'application/json'
          },
          body: JSON.stringify(data)
        });
        
        return {
          status: response.status,
          data: response.ok() ? await response.json() : await response.text()
        };
      }, invalidData);
      
      // Both should return 400 for validation errors
      expect(backendResponse.status()).toBe(400);
      expect(frontendResponse.status).toBe(400);
    });
  });
});
