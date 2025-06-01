# Test info

- Name: API Data Validation Tests >> Error Response Validation >> Validation error format consistency
- Location: /userdata/source/source/AISecurityVision_byaugment/tests/playwright/api-data-validation.spec.js:342:5

# Error details

```
Error: apiRequestContext.post: Request timed out after 30000ms
Call log:
  - → POST http://localhost:8080/api/cameras
    - user-agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/136.0.7103.25 Safari/537.36
    - accept: */*
    - accept-encoding: gzip,deflate,br
    - content-type: application/json
    - content-length: 25

    at /userdata/source/source/AISecurityVision_byaugment/tests/playwright/api-data-validation.spec.js:349:45
```

# Test source

```ts
  249 |       // Backend test
  250 |       const backendResponse = await request.get(`${API_BASE_URL}/cameras/${cameraId}/person-stats`);
  251 |       
  252 |       // Frontend test
  253 |       await page.goto(`${FRONTEND_URL}/person-statistics`);
  254 |       
  255 |       const frontendData = await page.evaluate(async (id) => {
  256 |         const response = await fetch(`/api/cameras/${id}/person-stats`);
  257 |         return {
  258 |           status: response.status,
  259 |           data: response.ok() ? await response.json() : null
  260 |         };
  261 |       }, cameraId);
  262 |       
  263 |       // Both should return same status
  264 |       expect(frontendData.status).toBe(backendResponse.status());
  265 |       
  266 |       // If successful, validate structure
  267 |       if (backendResponse.ok() && frontendData.data) {
  268 |         const backendData = await backendResponse.json();
  269 |         
  270 |         // Validate common structure
  271 |         const expectedFields = ['camera_id', 'enabled'];
  272 |         
  273 |         for (const field of expectedFields) {
  274 |           if (backendData.hasOwnProperty(field)) {
  275 |             expect(frontendData.data).toHaveProperty(field);
  276 |             expect(typeof backendData[field]).toBe(typeof frontendData.data[field]);
  277 |           }
  278 |         }
  279 |       }
  280 |     });
  281 |
  282 |     test('Person stats configuration validation', async ({ request, page }) => {
  283 |       const cameraId = 'camera_01';
  284 |       const configData = {
  285 |         enabled: true,
  286 |         age_detection: true,
  287 |         gender_detection: true,
  288 |         confidence_threshold: 0.7
  289 |       };
  290 |       
  291 |       // Backend test
  292 |       const backendResponse = await request.post(`${API_BASE_URL}/cameras/${cameraId}/person-stats/config`, {
  293 |         data: configData
  294 |       });
  295 |       
  296 |       // Frontend test
  297 |       await page.goto(`${FRONTEND_URL}/person-statistics`);
  298 |       
  299 |       const frontendResponse = await page.evaluate(async (id, config) => {
  300 |         const response = await fetch(`/api/cameras/${id}/person-stats/config`, {
  301 |           method: 'POST',
  302 |           headers: {
  303 |             'Content-Type': 'application/json'
  304 |           },
  305 |           body: JSON.stringify(config)
  306 |         });
  307 |         
  308 |         return {
  309 |           status: response.status,
  310 |           data: response.ok() ? await response.json() : null
  311 |         };
  312 |       }, cameraId, configData);
  313 |       
  314 |       // Both should handle configuration the same way
  315 |       expect(frontendResponse.status).toBe(backendResponse.status());
  316 |     });
  317 |   });
  318 |
  319 |   test.describe('Error Response Validation', () => {
  320 |     test('Error response format consistency', async ({ request, page }) => {
  321 |       const invalidEndpoint = '/api/invalid-endpoint';
  322 |       
  323 |       // Backend test
  324 |       const backendResponse = await request.get(`http://localhost:8080${invalidEndpoint}`);
  325 |       
  326 |       // Frontend test
  327 |       await page.goto(FRONTEND_URL);
  328 |       
  329 |       const frontendResponse = await page.evaluate(async (endpoint) => {
  330 |         const response = await fetch(endpoint);
  331 |         return {
  332 |           status: response.status,
  333 |           data: response.ok() ? await response.json() : null
  334 |         };
  335 |       }, invalidEndpoint);
  336 |       
  337 |       // Both should return 404
  338 |       expect(backendResponse.status()).toBe(404);
  339 |       expect(frontendResponse.status).toBe(404);
  340 |     });
  341 |
  342 |     test('Validation error format consistency', async ({ request, page }) => {
  343 |       const invalidData = {
  344 |         // Missing required fields
  345 |         name: 'Invalid Camera'
  346 |       };
  347 |       
  348 |       // Backend test
> 349 |       const backendResponse = await request.post(`${API_BASE_URL}/cameras`, {
      |                                             ^ Error: apiRequestContext.post: Request timed out after 30000ms
  350 |         data: invalidData
  351 |       });
  352 |       
  353 |       // Frontend test
  354 |       await page.goto(`${FRONTEND_URL}/cameras`);
  355 |       
  356 |       const frontendResponse = await page.evaluate(async (data) => {
  357 |         const response = await fetch('/api/cameras', {
  358 |           method: 'POST',
  359 |           headers: {
  360 |             'Content-Type': 'application/json'
  361 |           },
  362 |           body: JSON.stringify(data)
  363 |         });
  364 |         
  365 |         return {
  366 |           status: response.status,
  367 |           data: response.ok() ? await response.json() : await response.text()
  368 |         };
  369 |       }, invalidData);
  370 |       
  371 |       // Both should return 400 for validation errors
  372 |       expect(backendResponse.status()).toBe(400);
  373 |       expect(frontendResponse.status).toBe(400);
  374 |     });
  375 |   });
  376 | });
  377 |
```