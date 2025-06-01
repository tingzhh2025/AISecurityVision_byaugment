# Test info

- Name: API Endpoint Consistency Tests >> Error Handling Consistency >> 400 Errors - Invalid Request Data
- Location: /userdata/source/source/AISecurityVision_byaugment/tests/playwright/api-consistency.spec.js:357:5

# Error details

```
Error: expect(received).toBe(expected) // Object.is equality

Expected: 400
Received: 200
    at /userdata/source/source/AISecurityVision_byaugment/tests/playwright/api-consistency.spec.js:384:40
```

# Page snapshot

```yaml
- complementary:
  - img
  - text: AI安防监控
  - menubar:
    - menuitem "仪表盘":
      - img
      - text: 仪表盘
    - menuitem "实时监控":
      - img
      - text: 实时监控
    - menuitem "录像回放":
      - img
      - text: 录像回放
    - menuitem "报警管理":
      - img
      - text: 报警管理
    - menuitem "摄像头管理":
      - img
      - text: 摄像头管理
    - menuitem "系统设置":
      - img
      - text: 系统设置
- button:
  - img
- navigation "面包屑":
  - link "摄像头管理"
- text: 在线 2025-06-01 14:12:24
- button:
  - img
- main:
  - button "添加摄像头":
    - img
    - text: 添加摄像头
  - button "刷新":
    - img
    - text: 刷新
  - img
  - textbox "搜索摄像头名称或IP"
  - img "RTSP Camera 1 (192.168.1.3)"
  - text: 在线
  - heading "RTSP Camera 1 (192.168.1.3)" [level=3]
  - text: "IP地址: 分辨率: 帧率: fps AI检测: 禁用"
  - button "实时查看":
    - img
    - text: 实时查看
  - button "测试连接":
    - img
    - text: 测试连接
  - button "编辑":
    - img
    - text: 编辑
  - button "删除":
    - img
    - text: 删除
  - text: 在线
  - heading "RTSP Camera 2 (192.168.1.3)" [level=3]
  - text: "IP地址: 分辨率: 帧率: fps AI检测: 禁用"
  - button "实时查看":
    - img
    - text: 实时查看
  - button "测试连接":
    - img
    - text: 测试连接
  - button "编辑":
    - img
    - text: 编辑
  - button "删除":
    - img
    - text: 删除
  - text: 在线
  - heading "RTSP Camera 3 (192.168.1.3)" [level=3]
  - text: "IP地址: 分辨率: 帧率: fps AI检测: 禁用"
  - button "实时查看":
    - img
    - text: 实时查看
  - button "测试连接":
    - img
    - text: 测试连接
  - button "编辑":
    - img
    - text: 编辑
  - button "删除":
    - img
    - text: 删除
  - text: 在线
  - heading "RTSP Camera 4 (192.168.1.3)" [level=3]
  - text: "IP地址: 分辨率: 帧率: fps AI检测: 禁用"
  - button "实时查看":
    - img
    - text: 实时查看
  - button "测试连接":
    - img
    - text: 测试连接
  - button "编辑":
    - img
    - text: 编辑
  - button "删除":
    - img
    - text: 删除
  - text: 在线
  - heading "RTSP Camera 5 (192.168.1.2)" [level=3]
  - text: "IP地址: 分辨率: 帧率: fps AI检测: 禁用"
  - button "实时查看":
    - img
    - text: 实时查看
  - button "测试连接":
    - img
    - text: 测试连接
  - button "编辑":
    - img
    - text: 编辑
  - button "删除":
    - img
    - text: 删除
  - text: 在线
  - heading "RTSP Camera 6 (192.168.1.2)" [level=3]
  - text: "IP地址: 分辨率: 帧率: fps AI检测: 禁用"
  - button "实时查看":
    - img
    - text: 实时查看
  - button "测试连接":
    - img
    - text: 测试连接
  - button "编辑":
    - img
    - text: 编辑
  - button "删除":
    - img
    - text: 删除
  - text: 在线
  - heading "RTSP Camera 7 (192.168.1.2)" [level=3]
  - text: "IP地址: 分辨率: 帧率: fps AI检测: 禁用"
  - button "实时查看":
    - img
    - text: 实时查看
  - button "测试连接":
    - img
    - text: 测试连接
  - button "编辑":
    - img
    - text: 编辑
  - button "删除":
    - img
    - text: 删除
  - text: 在线
  - heading "RTSP Camera 8 (192.168.1.2)" [level=3]
  - text: "IP地址: 分辨率: 帧率: fps AI检测: 禁用"
  - button "实时查看":
    - img
    - text: 实时查看
  - button "测试连接":
    - img
    - text: 测试连接
  - button "编辑":
    - img
    - text: 编辑
  - button "删除":
    - img
    - text: 删除
```

# Test source

```ts
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
  311 |       const backendResponse = await request.post(`${API_BASE_URL}/cameras/${cameraId}/person-stats/enable`, {
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
> 384 |       expect(backendResponse.status()).toBe(400);
      |                                        ^ Error: expect(received).toBe(expected) // Object.is equality
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
  412 |         expect(backendContentType).toContain('application/json');
  413 |         expect(frontendContentType).toContain('application/json');
  414 |       }
  415 |     });
  416 |
  417 |     test('CORS Headers Consistency', async ({ request }) => {
  418 |       // Backend test
  419 |       const backendResponse = await request.get(`${API_BASE_URL}/system/status`);
  420 |       const corsHeader = backendResponse.headers()['access-control-allow-origin'];
  421 |
  422 |       // Frontend test
  423 |       await page.goto(FRONTEND_URL);
  424 |
  425 |       const frontendCors = await page.evaluate(async () => {
  426 |         const response = await fetch('/api/system/status');
  427 |         return response.headers.get('access-control-allow-origin');
  428 |       });
  429 |
  430 |       // Both should have CORS headers
  431 |       expect(corsHeader).toBe('*');
  432 |       expect(frontendCors).toBe('*');
  433 |     });
  434 |   });
  435 |
  436 |   test.describe('Real-time Features', () => {
  437 |     test('MJPEG Stream URLs Consistency', async ({ request }) => {
  438 |       // Test that frontend stream URL mapping matches backend capabilities
  439 |       await page.goto(`${FRONTEND_URL}/live`);
  440 |
  441 |       const streamUrls = await page.evaluate(() => {
  442 |         // Get stream URLs from frontend API service
  443 |         const streamMapping = {
  444 |           'camera_01': 'http://localhost:8161/stream.mjpg',
  445 |           'camera_02': 'http://localhost:8162/stream.mjpg',
  446 |           'camera_03': 'http://localhost:8163/stream.mjpg',
  447 |           'camera_04': 'http://localhost:8164/stream.mjpg'
  448 |         };
  449 |         return streamMapping;
  450 |       });
  451 |
  452 |       // Verify stream URLs are accessible (at least respond)
  453 |       for (const [cameraId, streamUrl] of Object.entries(streamUrls)) {
  454 |         try {
  455 |           const response = await request.get(streamUrl, { timeout: 5000 });
  456 |           console.log(`Stream ${cameraId}: ${response.status()}`);
  457 |           // Stream might return 404 if camera not active, but should not timeout
  458 |           expect([200, 404, 503].includes(response.status())).toBeTruthy();
  459 |         } catch (error) {
  460 |           console.log(`Stream ${cameraId} not accessible: ${error.message}`);
  461 |           // This is acceptable as cameras might not be running
  462 |         }
  463 |       }
  464 |     });
  465 |
  466 |     test('Live View Page Integration', async ({ request }) => {
  467 |       // Test that live view page can access camera data
  468 |       await page.goto(`${FRONTEND_URL}/live`);
  469 |
  470 |       // Wait for page to load
  471 |       await page.waitForLoadState('networkidle');
  472 |
  473 |       // Check if camera selection is available
  474 |       const cameraSelectExists = await page.locator('select, .el-select').count() > 0;
  475 |
  476 |       if (cameraSelectExists) {
  477 |         // Verify camera data is loaded from API
  478 |         const cameraData = await page.evaluate(async () => {
  479 |           try {
  480 |             const response = await fetch('/api/cameras');
  481 |             return await response.json();
  482 |           } catch (error) {
  483 |             return { error: error.message };
  484 |           }
```