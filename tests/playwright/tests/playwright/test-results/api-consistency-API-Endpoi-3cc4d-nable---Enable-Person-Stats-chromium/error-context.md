# Test info

- Name: API Endpoint Consistency Tests >> Person Statistics Endpoints >> POST /api/cameras/{id}/person-stats/enable - Enable Person Stats
- Location: /userdata/source/source/AISecurityVision_byaugment/tests/playwright/api-consistency.spec.js:306:5

# Error details

```
Error: apiRequestContext.post: Request timed out after 30000ms
Call log:
  - → POST http://localhost:8080/api/cameras/camera_01/person-stats/enable
    - user-agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/136.0.7103.25 Safari/537.36
    - accept: */*
    - accept-encoding: gzip,deflate,br
    - content-type: application/json
    - content-length: 16

    at /userdata/source/source/AISecurityVision_byaugment/tests/playwright/api-consistency.spec.js:311:45
```

# Test source

```ts
  211 |       // Frontend test
  212 |       await page.goto(`${FRONTEND_URL}/cameras`);
  213 |       
  214 |       const frontendCall = await page.evaluate(async (testData) => {
  215 |         const response = await fetch('/api/cameras/test-connection', {
  216 |           method: 'POST',
  217 |           headers: {
  218 |             'Content-Type': 'application/json'
  219 |           },
  220 |           body: JSON.stringify(testData)
  221 |         });
  222 |         
  223 |         return {
  224 |           status: response.status,
  225 |           data: await response.json()
  226 |         };
  227 |       }, connectionTest);
  228 |       
  229 |       // Both should return same status
  230 |       expect(frontendCall.status).toBe(backendResponse.status());
  231 |     });
  232 |   });
  233 |
  234 |   test.describe('Alert Endpoints', () => {
  235 |     test('GET /api/alerts - Alerts Consistency', async ({ request }) => {
  236 |       // Backend test
  237 |       const backendResponse = await request.get(`${API_BASE_URL}/alerts`);
  238 |       expect(backendResponse.ok()).toBeTruthy();
  239 |       
  240 |       const backendData = await backendResponse.json();
  241 |       
  242 |       // Frontend test through alerts page
  243 |       await page.goto(`${FRONTEND_URL}/alerts`);
  244 |       
  245 |       const frontendCall = await page.evaluate(async () => {
  246 |         const response = await fetch('/api/alerts');
  247 |         return {
  248 |           status: response.status,
  249 |           data: await response.json()
  250 |         };
  251 |       });
  252 |       
  253 |       expect(frontendCall.status).toBe(200);
  254 |       
  255 |       // Verify structure consistency
  256 |       if (backendData.alerts) {
  257 |         expect(frontendCall.data).toHaveProperty('alerts');
  258 |         expect(Array.isArray(frontendCall.data.alerts)).toBeTruthy();
  259 |       }
  260 |     });
  261 |   });
  262 |
  263 |   test.describe('Detection Configuration', () => {
  264 |     test('GET /api/detection/config - Detection Config Consistency', async ({ request }) => {
  265 |       // Backend test
  266 |       const backendResponse = await request.get(`${API_BASE_URL}/detection/config`);
  267 |       expect(backendResponse.ok()).toBeTruthy();
  268 |       
  269 |       // Frontend test
  270 |       await page.goto(`${FRONTEND_URL}/settings`);
  271 |       
  272 |       const frontendCall = await page.evaluate(async () => {
  273 |         const response = await fetch('/api/detection/config');
  274 |         return {
  275 |           status: response.status,
  276 |           data: await response.json()
  277 |         };
  278 |       });
  279 |       
  280 |       expect(frontendCall.status).toBe(backendResponse.status());
  281 |     });
  282 |   });
  283 |
  284 |   test.describe('Person Statistics Endpoints', () => {
  285 |     test('GET /api/cameras/{id}/person-stats - Person Stats Consistency', async ({ request }) => {
  286 |       const cameraId = 'camera_01';
  287 |       
  288 |       // Backend test
  289 |       const backendResponse = await request.get(`${API_BASE_URL}/cameras/${cameraId}/person-stats`);
  290 |       
  291 |       // Frontend test through person statistics page
  292 |       await page.goto(`${FRONTEND_URL}/person-statistics`);
  293 |       
  294 |       const frontendCall = await page.evaluate(async (id) => {
  295 |         const response = await fetch(`/api/cameras/${id}/person-stats`);
  296 |         return {
  297 |           status: response.status,
  298 |           data: response.ok() ? await response.json() : null
  299 |         };
  300 |       }, cameraId);
  301 |       
  302 |       // Both should return same status
  303 |       expect(frontendCall.status).toBe(backendResponse.status());
  304 |     });
  305 |
  306 |     test('POST /api/cameras/{id}/person-stats/enable - Enable Person Stats', async ({ request }) => {
  307 |       const cameraId = 'camera_01';
  308 |       const enableData = { enabled: true };
  309 |       
  310 |       // Backend test
> 311 |       const backendResponse = await request.post(`${API_BASE_URL}/cameras/${cameraId}/person-stats/enable`, {
      |                                             ^ Error: apiRequestContext.post: Request timed out after 30000ms
  312 |         data: enableData
  313 |       });
  314 |       
  315 |       // Frontend test
  316 |       await page.goto(`${FRONTEND_URL}/person-statistics`);
  317 |       
  318 |       const frontendCall = await page.evaluate(async (id, data) => {
  319 |         const response = await fetch(`/api/cameras/${id}/person-stats/enable`, {
  320 |           method: 'POST',
  321 |           headers: {
  322 |             'Content-Type': 'application/json'
  323 |           },
  324 |           body: JSON.stringify(data)
  325 |         });
  326 |         
  327 |         return {
  328 |           status: response.status,
  329 |           data: response.ok() ? await response.json() : null
  330 |         };
  331 |       }, cameraId, enableData);
  332 |       
  333 |       expect(frontendCall.status).toBe(backendResponse.status());
  334 |     });
  335 |   });
  336 |
  337 |   test.describe('Error Handling Consistency', () => {
  338 |     test('404 Errors - Non-existent Endpoints', async ({ request }) => {
  339 |       const nonExistentEndpoint = '/api/non-existent-endpoint';
  340 |       
  341 |       // Backend test
  342 |       const backendResponse = await request.get(`${API_BASE_URL.replace('/api', '')}${nonExistentEndpoint}`);
  343 |       
  344 |       // Frontend test
  345 |       await page.goto(FRONTEND_URL);
  346 |       
  347 |       const frontendCall = await page.evaluate(async (endpoint) => {
  348 |         const response = await fetch(endpoint);
  349 |         return response.status;
  350 |       }, nonExistentEndpoint);
  351 |       
  352 |       // Both should return 404
  353 |       expect(backendResponse.status()).toBe(404);
  354 |       expect(frontendCall).toBe(404);
  355 |     });
  356 |
  357 |     test('400 Errors - Invalid Request Data', async ({ request }) => {
  358 |       const invalidCameraData = {
  359 |         // Missing required fields
  360 |         name: 'Invalid Camera'
  361 |       };
  362 |       
  363 |       // Backend test
  364 |       const backendResponse = await request.post(`${API_BASE_URL}/cameras`, {
  365 |         data: invalidCameraData
  366 |       });
  367 |       
  368 |       // Frontend test
  369 |       await page.goto(`${FRONTEND_URL}/cameras`);
  370 |       
  371 |       const frontendCall = await page.evaluate(async (data) => {
  372 |         const response = await fetch('/api/cameras', {
  373 |           method: 'POST',
  374 |           headers: {
  375 |             'Content-Type': 'application/json'
  376 |           },
  377 |           body: JSON.stringify(data)
  378 |         });
  379 |         
  380 |         return response.status;
  381 |       }, invalidCameraData);
  382 |       
  383 |       // Both should return 400
  384 |       expect(backendResponse.status()).toBe(400);
  385 |       expect(frontendCall).toBe(400);
  386 |     });
  387 |   });
  388 |
  389 |   test.describe('Response Format Consistency', () => {
  390 |     test('JSON Response Headers', async ({ request }) => {
  391 |       const endpoints = [
  392 |         '/system/status',
  393 |         '/cameras',
  394 |         '/alerts',
  395 |         '/detection/config'
  396 |       ];
  397 |
  398 |       for (const endpoint of endpoints) {
  399 |         // Backend test
  400 |         const backendResponse = await request.get(`${API_BASE_URL}${endpoint}`);
  401 |         const backendContentType = backendResponse.headers()['content-type'];
  402 |
  403 |         // Frontend test
  404 |         await page.goto(FRONTEND_URL);
  405 |
  406 |         const frontendContentType = await page.evaluate(async (ep) => {
  407 |           const response = await fetch(`/api${ep}`);
  408 |           return response.headers.get('content-type');
  409 |         }, endpoint);
  410 |
  411 |         // Both should return JSON content type
```