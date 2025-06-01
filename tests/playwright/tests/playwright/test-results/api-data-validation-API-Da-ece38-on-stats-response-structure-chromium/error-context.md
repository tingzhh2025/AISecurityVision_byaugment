# Test info

- Name: API Data Validation Tests >> Person Statistics Data Validation >> Person stats response structure
- Location: /userdata/source/source/AISecurityVision_byaugment/tests/playwright/api-data-validation.spec.js:246:5

# Error details

```
Error: apiRequestContext.get: Request timed out after 30000ms
Call log:
  - → GET http://localhost:8080/api/cameras/camera_01/person-stats
    - user-agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/136.0.7103.25 Safari/537.36
    - accept: */*
    - accept-encoding: gzip,deflate,br

    at /userdata/source/source/AISecurityVision_byaugment/tests/playwright/api-data-validation.spec.js:250:45
```

# Test source

```ts
  150 |         }, testCase.data);
  151 |         
  152 |         // Both should reject invalid data with same status
  153 |         expect(backendResponse.status()).toBe(testCase.expectedStatus);
  154 |         expect(frontendResponse).toBe(testCase.expectedStatus);
  155 |         
  156 |         console.log(`✅ ${testCase.name}: Backend ${backendResponse.status()}, Frontend ${frontendResponse}`);
  157 |       }
  158 |     });
  159 |   });
  160 |
  161 |   test.describe('System Status Data Validation', () => {
  162 |     test('System status response structure', async ({ request, page }) => {
  163 |       // Backend test
  164 |       const backendResponse = await request.get(`${API_BASE_URL}/system/status`);
  165 |       expect(backendResponse.ok()).toBeTruthy();
  166 |       
  167 |       const backendData = await backendResponse.json();
  168 |       
  169 |       // Frontend test
  170 |       await page.goto(FRONTEND_URL);
  171 |       
  172 |       const frontendData = await page.evaluate(async () => {
  173 |         const response = await fetch('/api/system/status');
  174 |         return await response.json();
  175 |       });
  176 |       
  177 |       // Validate required fields
  178 |       const requiredFields = ['status', 'active_pipelines', 'cpu_usage'];
  179 |       
  180 |       for (const field of requiredFields) {
  181 |         expect(backendData).toHaveProperty(field);
  182 |         expect(frontendData).toHaveProperty(field);
  183 |         
  184 |         // Validate data types
  185 |         if (field === 'status') {
  186 |           expect(typeof backendData[field]).toBe('string');
  187 |           expect(typeof frontendData[field]).toBe('string');
  188 |         } else {
  189 |           expect(typeof backendData[field]).toBe('number');
  190 |           expect(typeof frontendData[field]).toBe('number');
  191 |         }
  192 |       }
  193 |       
  194 |       // Validate value ranges
  195 |       expect(backendData.cpu_usage).toBeGreaterThanOrEqual(0);
  196 |       expect(backendData.cpu_usage).toBeLessThanOrEqual(100);
  197 |       expect(frontendData.cpu_usage).toBeGreaterThanOrEqual(0);
  198 |       expect(frontendData.cpu_usage).toBeLessThanOrEqual(100);
  199 |       
  200 |       expect(backendData.active_pipelines).toBeGreaterThanOrEqual(0);
  201 |       expect(frontendData.active_pipelines).toBeGreaterThanOrEqual(0);
  202 |     });
  203 |   });
  204 |
  205 |   test.describe('Alert Data Validation', () => {
  206 |     test('Alert object structure consistency', async ({ request, page }) => {
  207 |       // Backend test
  208 |       const backendResponse = await request.get(`${API_BASE_URL}/alerts`);
  209 |       expect(backendResponse.ok()).toBeTruthy();
  210 |       
  211 |       const backendData = await backendResponse.json();
  212 |       
  213 |       // Frontend test
  214 |       await page.goto(`${FRONTEND_URL}/alerts`);
  215 |       
  216 |       const frontendData = await page.evaluate(async () => {
  217 |         const response = await fetch('/api/alerts');
  218 |         return await response.json();
  219 |       });
  220 |       
  221 |       // Validate structure
  222 |       expect(backendData).toHaveProperty('alerts');
  223 |       expect(frontendData).toHaveProperty('alerts');
  224 |       
  225 |       expect(Array.isArray(backendData.alerts)).toBeTruthy();
  226 |       expect(Array.isArray(frontendData.alerts)).toBeTruthy();
  227 |       
  228 |       // If alerts exist, validate structure
  229 |       if (backendData.alerts.length > 0 && frontendData.alerts.length > 0) {
  230 |         const backendAlert = backendData.alerts[0];
  231 |         const frontendAlert = frontendData.alerts[0];
  232 |         
  233 |         const expectedFields = ['id', 'title', 'level', 'timestamp'];
  234 |         
  235 |         for (const field of expectedFields) {
  236 |           if (backendAlert.hasOwnProperty(field)) {
  237 |             expect(frontendAlert).toHaveProperty(field);
  238 |             expect(typeof backendAlert[field]).toBe(typeof frontendAlert[field]);
  239 |           }
  240 |         }
  241 |       }
  242 |     });
  243 |   });
  244 |
  245 |   test.describe('Person Statistics Data Validation', () => {
  246 |     test('Person stats response structure', async ({ request, page }) => {
  247 |       const cameraId = 'camera_01';
  248 |       
  249 |       // Backend test
> 250 |       const backendResponse = await request.get(`${API_BASE_URL}/cameras/${cameraId}/person-stats`);
      |                                             ^ Error: apiRequestContext.get: Request timed out after 30000ms
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
  349 |       const backendResponse = await request.post(`${API_BASE_URL}/cameras`, {
  350 |         data: invalidData
```