#!/bin/bash

# AI Security Vision System - Network API Test Script
echo "=== AI Security Vision System - Network API Test ==="

BASE_URL="http://localhost:8080/api"

echo ""
echo "1. Testing Network Interfaces API..."
echo "GET $BASE_URL/network/interfaces"
curl -s "$BASE_URL/network/interfaces" | jq . || curl -s "$BASE_URL/network/interfaces"

echo ""
echo ""
echo "2. Testing Single Interface API (eth0)..."
echo "GET $BASE_URL/network/interfaces/eth0"
curl -s "$BASE_URL/network/interfaces/eth0" | jq . || curl -s "$BASE_URL/network/interfaces/eth0"

echo ""
echo ""
echo "3. Testing Network Stats API..."
echo "GET $BASE_URL/network/stats"
curl -s "$BASE_URL/network/stats" | jq . || curl -s "$BASE_URL/network/stats"

echo ""
echo ""
echo "4. Testing Network Connection Test API..."
echo "POST $BASE_URL/network/test"
curl -s -X POST "$BASE_URL/network/test" \
  -H "Content-Type: application/json" \
  -d '{"host":"8.8.8.8","timeout":5}' | jq . || \
curl -s -X POST "$BASE_URL/network/test" \
  -H "Content-Type: application/json" \
  -d '{"host":"8.8.8.8","timeout":5}'

echo ""
echo ""
echo "5. Testing Network Interface Configuration API..."
echo "POST $BASE_URL/network/interfaces/eth0"
curl -s -X POST "$BASE_URL/network/interfaces/eth0" \
  -H "Content-Type: application/json" \
  -d '{
    "enabled": true,
    "is_dhcp": false,
    "ip_address": "192.168.1.199",
    "netmask": "255.255.255.0",
    "gateway": "192.168.1.1",
    "dns1": "8.8.8.8",
    "dns2": "8.8.4.4"
  }' | jq . || \
curl -s -X POST "$BASE_URL/network/interfaces/eth0" \
  -H "Content-Type: application/json" \
  -d '{
    "enabled": true,
    "is_dhcp": false,
    "ip_address": "192.168.1.199",
    "netmask": "255.255.255.0",
    "gateway": "192.168.1.1",
    "dns1": "8.8.8.8",
    "dns2": "8.8.4.4"
  }'

echo ""
echo ""
echo "=== Network API Test Complete ==="
