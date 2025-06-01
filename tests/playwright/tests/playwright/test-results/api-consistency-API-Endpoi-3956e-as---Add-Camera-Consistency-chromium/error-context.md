# Test info

- Name: API Endpoint Consistency Tests >> Camera Endpoints >> POST /api/cameras - Add Camera Consistency
- Location: /userdata/source/source/AISecurityVision_byaugment/tests/playwright/api-consistency.spec.js:150:5

# Error details

```
Error: expect(received).toBeTruthy()

Received: false
    at /userdata/source/source/AISecurityVision_byaugment/tests/playwright/api-consistency.spec.js:171:61
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
- text: 离线 2025-06-01 14:12:32
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
  - img "Validation Test Camera"
  - text: 在线
  - heading "Validation Test Camera" [level=3]
  - text: "IP地址: 分辨率: 帧率: 25fps AI检测: 禁用"
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
- alert:
  - img
  - paragraph: 摄像头列表已刷新
```

# Test source

```ts
   71 |       // Test backend endpoint
   72 |       const backendResponse = await request.get(`${API_BASE_URL}/system/info`);
   73 |       expect(backendResponse.ok()).toBeTruthy();
   74 |       
   75 |       // Test frontend through dashboard page
   76 |       await page.goto(`${FRONTEND_URL}/dashboard`);
   77 |       
   78 |       // Wait for system info to load
   79 |       await page.waitForFunction(() => {
   80 |         return window.fetch !== undefined;
   81 |       });
   82 |       
   83 |       const frontendCall = await page.evaluate(async () => {
   84 |         const response = await fetch('/api/system/info');
   85 |         return {
   86 |           status: response.status,
   87 |           data: await response.json()
   88 |         };
   89 |       });
   90 |       
   91 |       expect(frontendCall.status).toBe(200);
   92 |       expect(frontendCall.data).toBeDefined();
   93 |     });
   94 |
   95 |     test('GET /api/system/config - Configuration Management', async ({ request }) => {
   96 |       // Backend test
   97 |       const backendResponse = await request.get(`${API_BASE_URL}/system/config`);
   98 |       expect(backendResponse.ok()).toBeTruthy();
   99 |       
  100 |       // Frontend test through settings page
  101 |       await page.goto(`${FRONTEND_URL}/settings`);
  102 |       
  103 |       const configResponse = await page.evaluate(async () => {
  104 |         try {
  105 |           const response = await fetch('/api/system/config');
  106 |           return {
  107 |             status: response.status,
  108 |             data: await response.json()
  109 |           };
  110 |         } catch (error) {
  111 |           return { error: error.message };
  112 |         }
  113 |       });
  114 |       
  115 |       expect(configResponse.status).toBe(200);
  116 |     });
  117 |   });
  118 |
  119 |   test.describe('Camera Endpoints', () => {
  120 |     test('GET /api/cameras - Camera List Consistency', async ({ request }) => {
  121 |       // Backend test
  122 |       const backendResponse = await request.get(`${API_BASE_URL}/cameras`);
  123 |       expect(backendResponse.ok()).toBeTruthy();
  124 |       
  125 |       const backendData = await backendResponse.json();
  126 |       
  127 |       // Frontend test through cameras page
  128 |       await page.goto(`${FRONTEND_URL}/cameras`);
  129 |       
  130 |       // Wait for cameras to load
  131 |       await page.waitForSelector('.cameras-grid', { timeout: 10000 });
  132 |       
  133 |       const frontendCall = await page.evaluate(async () => {
  134 |         const response = await fetch('/api/cameras');
  135 |         return {
  136 |           status: response.status,
  137 |           data: await response.json()
  138 |         };
  139 |       });
  140 |       
  141 |       expect(frontendCall.status).toBe(200);
  142 |       
  143 |       // Verify structure consistency
  144 |       if (backendData.cameras) {
  145 |         expect(frontendCall.data).toHaveProperty('cameras');
  146 |         expect(Array.isArray(frontendCall.data.cameras)).toBeTruthy();
  147 |       }
  148 |     });
  149 |
  150 |     test('POST /api/cameras - Add Camera Consistency', async ({ request }) => {
  151 |       const testCamera = {
  152 |         id: 'test_camera_playwright',
  153 |         name: 'Playwright Test Camera',
  154 |         url: 'rtsp://test:test@192.168.1.100:554/stream',
  155 |         protocol: 'rtsp',
  156 |         username: 'test',
  157 |         password: 'test',
  158 |         width: 1920,
  159 |         height: 1080,
  160 |         fps: 25,
  161 |         mjpeg_port: 8199,
  162 |         enabled: true
  163 |       };
  164 |       
  165 |       // Test backend endpoint directly
  166 |       const backendResponse = await request.post(`${API_BASE_URL}/cameras`, {
  167 |         data: testCamera
  168 |       });
  169 |       
  170 |       // Should return 201 for created or 409 if already exists
> 171 |       expect([201, 409].includes(backendResponse.status())).toBeTruthy();
      |                                                             ^ Error: expect(received).toBeTruthy()
  172 |       
  173 |       // Test frontend API call
  174 |       await page.goto(`${FRONTEND_URL}/cameras`);
  175 |       
  176 |       const frontendCall = await page.evaluate(async (camera) => {
  177 |         try {
  178 |           const response = await fetch('/api/cameras', {
  179 |             method: 'POST',
  180 |             headers: {
  181 |               'Content-Type': 'application/json'
  182 |             },
  183 |             body: JSON.stringify(camera)
  184 |           });
  185 |           
  186 |           return {
  187 |             status: response.status,
  188 |             data: await response.json()
  189 |           };
  190 |         } catch (error) {
  191 |           return { error: error.message };
  192 |         }
  193 |       }, testCamera);
  194 |       
  195 |       // Should handle the same way as backend
  196 |       expect([201, 409].includes(frontendCall.status)).toBeTruthy();
  197 |     });
  198 |
  199 |     test('POST /api/cameras/test-connection - Connection Test', async ({ request }) => {
  200 |       const connectionTest = {
  201 |         url: 'rtsp://admin:sharpi1688@192.168.1.3:554/1/1',
  202 |         username: 'admin',
  203 |         password: 'sharpi1688'
  204 |       };
  205 |       
  206 |       // Backend test
  207 |       const backendResponse = await request.post(`${API_BASE_URL}/cameras/test-connection`, {
  208 |         data: connectionTest
  209 |       });
  210 |       
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
```