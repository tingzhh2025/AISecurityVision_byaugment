const { test, expect } = require('@playwright/test');

test.describe('Basic API Tests', () => {
  test('Backend Camera API Test', async ({ request }) => {
    console.log('Testing backend camera API...');
    
    const response = await request.get('http://127.0.0.1:8080/api/cameras');
    console.log(`Response status: ${response.status()}`);
    
    expect(response.status()).toBe(200);
    
    const data = await response.json();
    console.log('Response data:', JSON.stringify(data, null, 2));
    
    expect(data).toHaveProperty('cameras');
    expect(data).toHaveProperty('total');
    expect(Array.isArray(data.cameras)).toBe(true);
  });

  test('Backend Alerts API Test', async ({ request }) => {
    console.log('Testing backend alerts API...');
    
    const response = await request.get('http://127.0.0.1:8080/api/alerts');
    console.log(`Response status: ${response.status()}`);
    
    expect(response.status()).toBe(200);
    
    const data = await response.json();
    console.log('Response data:', JSON.stringify(data, null, 2));
    
    expect(data).toHaveProperty('alerts');
    expect(data).toHaveProperty('total');
    expect(Array.isArray(data.alerts)).toBe(true);
  });

  test('Backend System Status API Test', async ({ request }) => {
    console.log('Testing backend system status API...');
    
    const response = await request.get('http://127.0.0.1:8080/api/system/status');
    console.log(`Response status: ${response.status()}`);
    
    expect(response.status()).toBe(200);
    
    const data = await response.json();
    console.log('Response data:', JSON.stringify(data, null, 2));
    
    expect(data).toHaveProperty('status');
    expect(data).toHaveProperty('active_pipelines');
    expect(data).toHaveProperty('cpu_usage');
    expect(typeof data.cpu_usage).toBe('number');
  });

  test('Frontend Access Test', async ({ page }) => {
    console.log('Testing frontend access...');
    
    await page.goto('http://127.0.0.1:3000');
    
    const title = await page.title();
    console.log('Page title:', title);
    
    expect(title).toBeDefined();
    
    // Check if Vue app is loaded
    const appElement = await page.locator('#app').count();
    console.log('Vue app element found:', appElement > 0);
    
    expect(appElement).toBeGreaterThan(0);
  });

  test('Frontend API Proxy Test', async ({ page }) => {
    console.log('Testing frontend API proxy...');
    
    await page.goto('http://127.0.0.1:3000');
    
    // Test API call through frontend proxy
    const apiResponse = await page.evaluate(async () => {
      try {
        const response = await fetch('/api/cameras');
        const data = await response.json();
        return {
          status: response.status,
          ok: response.ok,
          data: data
        };
      } catch (error) {
        return {
          status: 0,
          ok: false,
          error: error.message
        };
      }
    });
    
    console.log('Frontend API response:', JSON.stringify(apiResponse, null, 2));
    
    expect(apiResponse.ok).toBe(true);
    expect(apiResponse.status).toBe(200);
    expect(apiResponse.data).toHaveProperty('cameras');
  });

  test('API Consistency Test', async ({ page, request }) => {
    console.log('Testing API consistency between frontend and backend...');
    
    // Get data from backend directly
    const backendResponse = await request.get('http://127.0.0.1:8080/api/cameras');
    const backendData = await backendResponse.json();
    console.log('Backend data:', JSON.stringify(backendData, null, 2));
    
    // Get data from frontend proxy
    await page.goto('http://127.0.0.1:3000');
    
    const frontendData = await page.evaluate(async () => {
      const response = await fetch('/api/cameras');
      return await response.json();
    });
    console.log('Frontend data:', JSON.stringify(frontendData, null, 2));
    
    // Compare structure
    expect(frontendData).toHaveProperty('cameras');
    expect(frontendData).toHaveProperty('total');
    expect(backendData).toHaveProperty('cameras');
    expect(backendData).toHaveProperty('total');
    
    // Compare values
    expect(frontendData.total).toBe(backendData.total);
    expect(frontendData.cameras.length).toBe(backendData.cameras.length);
    
    console.log('✅ API consistency verified!');
  });
});
