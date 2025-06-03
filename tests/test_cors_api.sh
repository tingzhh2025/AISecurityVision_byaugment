#!/bin/bash

echo "=== 人员统计配置API CORS测试 ==="
echo

# 测试1: 检查OPTIONS请求（CORS预检）
echo "测试1: OPTIONS预检请求..."
curl -s -X OPTIONS "http://localhost:8080/api/cameras/test_camera/person-stats/config" \
  -H "Origin: http://localhost:3001" \
  -H "Access-Control-Request-Method: POST" \
  -H "Access-Control-Request-Headers: Content-Type" \
  -v 2>&1 | grep -E "(Access-Control|HTTP/)"

echo
echo "测试2: GET请求..."
curl -s "http://localhost:8080/api/cameras/test_camera/person-stats/config" \
  -H "Origin: http://localhost:3001" \
  -v 2>&1 | grep -E "(Access-Control|HTTP/)" | head -5

echo
echo "测试3: POST请求..."
curl -s -X POST "http://localhost:8080/api/cameras/test_camera/person-stats/config" \
  -H "Content-Type: application/json" \
  -H "Origin: http://localhost:3001" \
  -d '{"enabled":true,"gender_threshold":0.9,"age_threshold":0.8,"batch_size":12,"enable_caching":true}' \
  -v 2>&1 | grep -E "(Access-Control|HTTP/)" | head -5

echo
echo "测试4: 验证配置更新..."
RESPONSE=$(curl -s "http://localhost:8080/api/cameras/test_camera/person-stats/config")
echo "当前配置: $RESPONSE"

echo
echo "=== 测试完成 ==="
