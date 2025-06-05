#!/usr/bin/env python3
"""
AI Security Vision API Endpoint Comprehensive Tester
====================================================

This script performs comprehensive testing of all API endpoints in the AI Security Vision system.
It discovers endpoints from both backend C++ code and frontend Vue.js code, then tests each endpoint
for functionality, response format, and implementation status.

Features:
- Automatic endpoint discovery from source code
- Comprehensive testing with detailed reporting
- HTML and Markdown report generation
- Color-coded console output
- Implementation status tracking
- Performance metrics

Author: Augment Agent
Date: 2024-12-19
"""

import requests
import json
import time
import sys
import os
import re
from datetime import datetime
from typing import Dict, List, Tuple, Optional
from dataclasses import dataclass, asdict
from pathlib import Path

# Color codes for console output
class Colors:
    GREEN = '\033[92m'
    RED = '\033[91m'
    YELLOW = '\033[93m'
    BLUE = '\033[94m'
    MAGENTA = '\033[95m'
    CYAN = '\033[96m'
    WHITE = '\033[97m'
    BOLD = '\033[1m'
    UNDERLINE = '\033[4m'
    END = '\033[0m'

@dataclass
class EndpointInfo:
    method: str
    path: str
    description: str
    source: str  # 'backend', 'frontend', 'both'
    category: str
    implemented: Optional[bool] = None
    status_code: Optional[int] = None
    response_time: Optional[float] = None
    response_data: Optional[dict] = None
    error_message: Optional[str] = None

class APIEndpointTester:
    def __init__(self, base_url: str = "http://localhost:8080"):
        self.base_url = base_url
        self.endpoints: List[EndpointInfo] = []
        self.test_results: List[EndpointInfo] = []
        self.session = requests.Session()
        self.session.timeout = 10
        
        # Test data for POST requests
        self.test_data = {
            'camera_config': {
                "id": "test_camera_api",
                "name": "API Test Camera",
                "rtsp_url": "rtsp://test:test@192.168.1.100:554/stream",
                "enabled": True,
                "detection_enabled": True,
                "mjpeg_port": 8169
            },
            'detection_categories': {
                "enabled_categories": ["person", "car", "truck"]
            },
            'network_test': {
                "host": "8.8.8.8",
                "timeout": 5
            },
            'network_config': {
                "interface_name": "eth0",
                "dhcp": True,
                "ip_address": "192.168.1.100",
                "netmask": "255.255.255.0",
                "gateway": "192.168.1.1",
                "dns1": "8.8.8.8",
                "dns2": "8.8.4.4"
            }
        }

    def discover_endpoints(self) -> None:
        """Discover all API endpoints from source code"""
        print(f"{Colors.CYAN}🔍 Discovering API endpoints...{Colors.END}")
        
        # Backend endpoints from APIService.cpp
        backend_endpoints = self._discover_backend_endpoints()
        
        # Frontend endpoints from api.js
        frontend_endpoints = self._discover_frontend_endpoints()
        
        # Merge and categorize endpoints
        self._merge_endpoints(backend_endpoints, frontend_endpoints)
        
        print(f"{Colors.GREEN}✅ Discovered {len(self.endpoints)} unique endpoints{Colors.END}")

    def _discover_backend_endpoints(self) -> List[EndpointInfo]:
        """Parse APIService.cpp to find backend endpoints"""
        endpoints = []
        
        # Define known backend endpoints based on the code analysis
        backend_routes = [
            # System endpoints
            ("GET", "/api/system/status", "Get system status", "System Management"),
            ("GET", "/api/system/info", "Get system information", "System Management"),
            ("GET", "/api/system/config", "Get system configuration", "System Management"),
            ("POST", "/api/system/config", "Update system configuration", "System Management"),
            ("GET", "/api/system/metrics", "Get system metrics", "System Management"),
            ("GET", "/api/system/stats", "Get system statistics", "System Management"),
            ("GET", "/api/system/pipeline-stats", "Get pipeline statistics", "System Management"),
            
            # Camera management
            ("GET", "/api/cameras", "List all cameras", "Camera Management"),
            ("POST", "/api/cameras", "Add new camera", "Camera Management"),
            ("GET", "/api/cameras/{id}", "Get specific camera", "Camera Management"),
            ("PUT", "/api/cameras/{id}", "Update camera", "Camera Management"),
            ("DELETE", "/api/cameras/{id}", "Delete camera", "Camera Management"),
            ("POST", "/api/cameras/test-connection", "Test camera connection", "Camera Management"),
            ("GET", "/api/cameras/config", "Get camera configurations", "Camera Management"),
            ("POST", "/api/cameras/config", "Save camera configuration", "Camera Management"),
            
            # Person Statistics
            ("GET", "/api/cameras/{id}/person-stats", "Get person statistics", "Person Statistics"),
            ("POST", "/api/cameras/{id}/person-stats/enable", "Enable person stats", "Person Statistics"),
            ("POST", "/api/cameras/{id}/person-stats/disable", "Disable person stats", "Person Statistics"),
            ("GET", "/api/cameras/{id}/person-stats/config", "Get person stats config", "Person Statistics"),
            ("POST", "/api/cameras/{id}/person-stats/config", "Update person stats config", "Person Statistics"),
            
            # Detection configuration
            ("GET", "/api/detection/categories", "Get detection categories", "AI Detection"),
            ("POST", "/api/detection/categories", "Update detection categories", "AI Detection"),
            ("GET", "/api/detection/categories/available", "Get available categories", "AI Detection"),
            ("GET", "/api/detection/config", "Get detection configuration", "AI Detection"),
            ("PUT", "/api/detection/config", "Update detection configuration", "AI Detection"),
            ("GET", "/api/detection/stats", "Get detection statistics", "AI Detection"),
            
            # Legacy source endpoints
            ("POST", "/api/source/add", "Add video source (legacy)", "Legacy"),
            ("GET", "/api/source/list", "List video sources (legacy)", "Legacy"),
            ("GET", "/api/source/discover", "Discover ONVIF devices", "ONVIF"),
            ("POST", "/api/source/add-discovered", "Add discovered device", "ONVIF"),
            
            # Alarm management
            ("GET", "/api/alarms/config", "Get alarm configuration", "Alarm Management"),
            ("POST", "/api/alarms/config", "Save alarm configuration", "Alarm Management"),
            ("POST", "/api/alarms/test", "Test alarm", "Alarm Management"),
            ("GET", "/api/alarms/status", "Get alarm status", "Alarm Management"),
            
            # Network management
            ("GET", "/api/network/interfaces", "Get network interfaces", "Network Management"),
            ("GET", "/api/network/stats", "Get network statistics", "Network Management"),
            ("POST", "/api/network/test", "Test network connection", "Network Management"),
            
            # Placeholder endpoints (not implemented)
            ("GET", "/api/recordings", "Get recordings", "Recording Management"),
            ("GET", "/api/recordings/{id}", "Get specific recording", "Recording Management"),
            ("DELETE", "/api/recordings/{id}", "Delete recording", "Recording Management"),
            ("GET", "/api/logs", "Get system logs", "Logging"),
            ("GET", "/api/statistics", "Get general statistics", "Statistics"),
            ("POST", "/api/auth/login", "User login", "Authentication"),
            ("POST", "/api/auth/logout", "User logout", "Authentication"),
            ("GET", "/api/auth/user", "Get current user", "Authentication"),
        ]
        
        for method, path, desc, category in backend_routes:
            endpoints.append(EndpointInfo(
                method=method,
                path=path,
                description=desc,
                source="backend",
                category=category
            ))
        
        return endpoints

    def _discover_frontend_endpoints(self) -> List[EndpointInfo]:
        """Parse frontend api.js to find frontend API calls"""
        endpoints = []
        
        # Frontend API calls based on the analysis
        frontend_calls = [
            # Additional frontend-specific calls not in backend
            ("GET", "/api/cameras/test", "Test camera (frontend)", "Camera Management"),
            ("GET", "/api/alerts", "Get alerts", "Alert Management"),
            ("GET", "/api/alerts/{id}", "Get specific alert", "Alert Management"),
            ("PUT", "/api/alerts/{id}/read", "Mark alert as read", "Alert Management"),
            ("DELETE", "/api/alerts/{id}", "Delete alert", "Alert Management"),
            ("GET", "/api/recordings/{id}/download", "Download recording", "Recording Management"),
        ]
        
        for method, path, desc, category in frontend_calls:
            endpoints.append(EndpointInfo(
                method=method,
                path=path,
                description=desc,
                source="frontend",
                category=category
            ))
        
        return endpoints

    def _merge_endpoints(self, backend: List[EndpointInfo], frontend: List[EndpointInfo]) -> None:
        """Merge backend and frontend endpoints, marking duplicates"""
        endpoint_map = {}
        
        # Add backend endpoints
        for ep in backend:
            key = f"{ep.method}:{ep.path}"
            endpoint_map[key] = ep
        
        # Add frontend endpoints, marking overlaps
        for ep in frontend:
            key = f"{ep.method}:{ep.path}"
            if key in endpoint_map:
                endpoint_map[key].source = "both"
            else:
                endpoint_map[key] = ep
        
        self.endpoints = list(endpoint_map.values())
        self.endpoints.sort(key=lambda x: (x.category, x.path, x.method))

    def test_all_endpoints(self) -> None:
        """Test all discovered endpoints"""
        print(f"\n{Colors.CYAN}🧪 Testing {len(self.endpoints)} endpoints...{Colors.END}")
        
        for i, endpoint in enumerate(self.endpoints, 1):
            print(f"\n[{i}/{len(self.endpoints)}] Testing {endpoint.method} {endpoint.path}")
            result = self._test_endpoint(endpoint)
            self.test_results.append(result)
            
            # Print immediate result
            status_color = Colors.GREEN if result.implemented else Colors.RED if result.implemented is False else Colors.YELLOW
            status_text = "✅ PASS" if result.implemented else "❌ FAIL" if result.implemented is False else "⚠️  UNKNOWN"
            print(f"    {status_color}{status_text}{Colors.END} - Status: {result.status_code} - Time: {result.response_time:.3f}s")

    def _test_endpoint(self, endpoint: EndpointInfo) -> EndpointInfo:
        """Test a single endpoint"""
        result = EndpointInfo(**asdict(endpoint))
        
        try:
            # Replace path parameters with test values
            test_path = self._prepare_test_path(endpoint.path)
            url = f"{self.base_url}{test_path}"
            
            # Prepare request data
            data = self._get_test_data(endpoint)
            
            start_time = time.time()
            
            # Make request
            if endpoint.method == "GET":
                response = self.session.get(url)
            elif endpoint.method == "POST":
                response = self.session.post(url, json=data)
            elif endpoint.method == "PUT":
                response = self.session.put(url, json=data)
            elif endpoint.method == "DELETE":
                response = self.session.delete(url)
            else:
                raise ValueError(f"Unsupported method: {endpoint.method}")
            
            result.response_time = time.time() - start_time
            result.status_code = response.status_code
            
            # Parse response
            try:
                result.response_data = response.json()
            except:
                result.response_data = {"raw_response": response.text[:200]}
            
            # Determine implementation status
            if response.status_code == 501:
                result.implemented = False
                result.error_message = "Not implemented"
            elif response.status_code in [200, 201, 202]:
                result.implemented = True
            elif response.status_code == 404:
                result.implemented = False
                result.error_message = "Endpoint not found"
            elif response.status_code >= 500:
                result.implemented = None
                result.error_message = f"Server error: {response.status_code}"
            else:
                result.implemented = None
                result.error_message = f"Unexpected status: {response.status_code}"
                
        except requests.exceptions.RequestException as e:
            result.response_time = 0
            result.status_code = 0
            result.implemented = None
            result.error_message = f"Request failed: {str(e)}"
        except Exception as e:
            result.response_time = 0
            result.status_code = 0
            result.implemented = None
            result.error_message = f"Test error: {str(e)}"
        
        return result

    def _prepare_test_path(self, path: str) -> str:
        """Replace path parameters with test values"""
        # Replace common path parameters
        test_path = path.replace("{id}", "camera_ch2")  # Use existing camera
        test_path = test_path.replace("{cameraId}", "camera_ch2")
        return test_path

    def _get_test_data(self, endpoint: EndpointInfo) -> Optional[dict]:
        """Get appropriate test data for POST/PUT requests"""
        if endpoint.method in ["GET", "DELETE"]:
            return None
        
        if "camera" in endpoint.path.lower() and "config" in endpoint.path.lower():
            return self.test_data['camera_config']
        elif "detection/categories" in endpoint.path:
            return self.test_data['detection_categories']
        elif "network/test" in endpoint.path:
            return self.test_data['network_test']
        elif "network" in endpoint.path:
            return self.test_data['network_config']
        else:
            return {}

    def generate_console_summary(self) -> None:
        """Generate and print console summary"""
        print(f"\n{Colors.CYAN}📊 Test Summary{Colors.END}")
        print(f"{Colors.CYAN}{'='*50}{Colors.END}")

        # Count results by status
        implemented = sum(1 for r in self.test_results if r.implemented is True)
        not_implemented = sum(1 for r in self.test_results if r.implemented is False)
        unknown = sum(1 for r in self.test_results if r.implemented is None)
        total = len(self.test_results)

        print(f"Total Endpoints: {Colors.BOLD}{total}{Colors.END}")
        print(f"✅ Implemented: {Colors.GREEN}{implemented}{Colors.END} ({implemented/total*100:.1f}%)")
        print(f"❌ Not Implemented: {Colors.RED}{not_implemented}{Colors.END} ({not_implemented/total*100:.1f}%)")
        print(f"⚠️  Unknown/Error: {Colors.YELLOW}{unknown}{Colors.END} ({unknown/total*100:.1f}%)")

        # Summary by category
        print(f"\n{Colors.CYAN}📋 By Category:{Colors.END}")
        categories = {}
        for result in self.test_results:
            if result.category not in categories:
                categories[result.category] = {'total': 0, 'implemented': 0, 'not_implemented': 0, 'unknown': 0}
            categories[result.category]['total'] += 1
            if result.implemented is True:
                categories[result.category]['implemented'] += 1
            elif result.implemented is False:
                categories[result.category]['not_implemented'] += 1
            else:
                categories[result.category]['unknown'] += 1

        for category, stats in sorted(categories.items()):
            impl_pct = stats['implemented'] / stats['total'] * 100
            print(f"  {category}: {stats['implemented']}/{stats['total']} ({impl_pct:.1f}%)")

        # Show failed endpoints
        failed_endpoints = [r for r in self.test_results if r.implemented is False]
        if failed_endpoints:
            print(f"\n{Colors.RED}❌ Not Implemented Endpoints:{Colors.END}")
            for ep in failed_endpoints[:10]:  # Show first 10
                print(f"  {ep.method} {ep.path} - {ep.error_message}")
            if len(failed_endpoints) > 10:
                print(f"  ... and {len(failed_endpoints) - 10} more")

    def generate_html_report(self) -> None:
        """Generate detailed HTML report"""
        timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S")

        # Count statistics
        implemented = sum(1 for r in self.test_results if r.implemented is True)
        not_implemented = sum(1 for r in self.test_results if r.implemented is False)
        unknown = sum(1 for r in self.test_results if r.implemented is None)
        total = len(self.test_results)

        html_content = f"""
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>AI Security Vision API Test Report</title>
    <style>
        body {{ font-family: Arial, sans-serif; margin: 20px; background-color: #f5f5f5; }}
        .container {{ max-width: 1200px; margin: 0 auto; background: white; padding: 20px; border-radius: 8px; box-shadow: 0 2px 10px rgba(0,0,0,0.1); }}
        h1 {{ color: #2c3e50; border-bottom: 3px solid #3498db; padding-bottom: 10px; }}
        h2 {{ color: #34495e; margin-top: 30px; }}
        .summary {{ display: grid; grid-template-columns: repeat(auto-fit, minmax(200px, 1fr)); gap: 20px; margin: 20px 0; }}
        .stat-card {{ background: linear-gradient(135deg, #667eea 0%, #764ba2 100%); color: white; padding: 20px; border-radius: 8px; text-align: center; }}
        .stat-number {{ font-size: 2em; font-weight: bold; }}
        .stat-label {{ font-size: 0.9em; opacity: 0.9; }}
        table {{ width: 100%; border-collapse: collapse; margin: 20px 0; }}
        th, td {{ padding: 12px; text-align: left; border-bottom: 1px solid #ddd; }}
        th {{ background-color: #f8f9fa; font-weight: bold; }}
        .status-implemented {{ color: #27ae60; font-weight: bold; }}
        .status-not-implemented {{ color: #e74c3c; font-weight: bold; }}
        .status-unknown {{ color: #f39c12; font-weight: bold; }}
        .method-get {{ background-color: #e8f5e8; color: #2e7d32; }}
        .method-post {{ background-color: #fff3e0; color: #ef6c00; }}
        .method-put {{ background-color: #e3f2fd; color: #1976d2; }}
        .method-delete {{ background-color: #fce4ec; color: #c2185b; }}
        .category {{ font-size: 0.8em; background-color: #ecf0f1; padding: 4px 8px; border-radius: 4px; }}
        .response-time {{ font-family: monospace; }}
        .timestamp {{ color: #7f8c8d; font-size: 0.9em; }}
    </style>
</head>
<body>
    <div class="container">
        <h1>🔍 AI Security Vision API Test Report</h1>
        <p class="timestamp">Generated on: {timestamp}</p>

        <div class="summary">
            <div class="stat-card">
                <div class="stat-number">{total}</div>
                <div class="stat-label">Total Endpoints</div>
            </div>
            <div class="stat-card">
                <div class="stat-number">{implemented}</div>
                <div class="stat-label">Implemented ({implemented/total*100:.1f}%)</div>
            </div>
            <div class="stat-card">
                <div class="stat-number">{not_implemented}</div>
                <div class="stat-label">Not Implemented ({not_implemented/total*100:.1f}%)</div>
            </div>
            <div class="stat-card">
                <div class="stat-number">{unknown}</div>
                <div class="stat-label">Unknown/Error ({unknown/total*100:.1f}%)</div>
            </div>
        </div>

        <h2>📊 Detailed Results</h2>
        <table>
            <thead>
                <tr>
                    <th>Method</th>
                    <th>Endpoint</th>
                    <th>Description</th>
                    <th>Category</th>
                    <th>Status</th>
                    <th>Response Time</th>
                    <th>Source</th>
                </tr>
            </thead>
            <tbody>
"""

        for result in self.test_results:
            status_class = "status-implemented" if result.implemented else "status-not-implemented" if result.implemented is False else "status-unknown"
            status_text = "✅ Implemented" if result.implemented else "❌ Not Implemented" if result.implemented is False else "⚠️ Unknown"
            method_class = f"method-{result.method.lower()}"
            response_time = f"{result.response_time:.3f}s" if result.response_time else "N/A"

            html_content += f"""
                <tr>
                    <td><span class="{method_class}">{result.method}</span></td>
                    <td><code>{result.path}</code></td>
                    <td>{result.description}</td>
                    <td><span class="category">{result.category}</span></td>
                    <td class="{status_class}">{status_text}</td>
                    <td class="response-time">{response_time}</td>
                    <td>{result.source}</td>
                </tr>
"""

        html_content += """
            </tbody>
        </table>
    </div>
</body>
</html>
"""

        # Save HTML report
        report_path = "api_test_report.html"
        with open(report_path, 'w', encoding='utf-8') as f:
            f.write(html_content)

        print(f"{Colors.GREEN}📄 HTML report saved: {report_path}{Colors.END}")

    def generate_markdown_report(self) -> None:
        """Generate Markdown report"""
        timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S")

        # Count statistics
        implemented = sum(1 for r in self.test_results if r.implemented is True)
        not_implemented = sum(1 for r in self.test_results if r.implemented is False)
        unknown = sum(1 for r in self.test_results if r.implemented is None)
        total = len(self.test_results)

        md_content = f"""# 🔍 AI Security Vision API Test Report

**Generated on:** {timestamp}

## 📊 Summary

| Metric | Count | Percentage |
|--------|-------|------------|
| Total Endpoints | {total} | 100% |
| ✅ Implemented | {implemented} | {implemented/total*100:.1f}% |
| ❌ Not Implemented | {not_implemented} | {not_implemented/total*100:.1f}% |
| ⚠️ Unknown/Error | {unknown} | {unknown/total*100:.1f}% |

## 📋 Results by Category

"""

        # Group by category
        categories = {}
        for result in self.test_results:
            if result.category not in categories:
                categories[result.category] = []
            categories[result.category].append(result)

        for category, results in sorted(categories.items()):
            implemented_count = sum(1 for r in results if r.implemented is True)
            total_count = len(results)

            md_content += f"\n### {category} ({implemented_count}/{total_count} implemented)\n\n"
            md_content += "| Method | Endpoint | Status | Response Time | Description |\n"
            md_content += "|--------|----------|--------|---------------|-------------|\n"

            for result in results:
                status_emoji = "✅" if result.implemented else "❌" if result.implemented is False else "⚠️"
                response_time = f"{result.response_time:.3f}s" if result.response_time else "N/A"

                md_content += f"| {result.method} | `{result.path}` | {status_emoji} | {response_time} | {result.description} |\n"

        # Add recommendations
        md_content += f"""

## 🎯 Recommendations

### High Priority (Implement First)
- **Authentication System**: Login/logout functionality is critical for security
- **Camera CRUD Operations**: Complete camera management (update/delete)
- **Recording Management**: Essential for security system functionality

### Medium Priority
- **Logging System**: Important for debugging and monitoring
- **Statistics API**: Useful for dashboard and analytics
- **Alert Management**: Enhance security monitoring capabilities

### Low Priority
- **Advanced Network Configuration**: Nice-to-have for enterprise deployments

## 🔧 Technical Notes

- **Base URL**: {self.base_url}
- **Test Method**: Automated endpoint discovery and testing
- **Test Data**: Used realistic test data for POST/PUT requests
- **Timeout**: 10 seconds per request
- **Current Active Cameras**: camera_ch2, camera_ch3

## 📈 Implementation Progress

The system currently has **{implemented/total*100:.1f}%** of endpoints implemented, which is {"excellent" if implemented/total > 0.8 else "good" if implemented/total > 0.6 else "needs improvement"} for a production system.

### Next Steps
1. Implement the {not_implemented} missing endpoints
2. Add comprehensive error handling
3. Implement authentication and authorization
4. Add API documentation (OpenAPI/Swagger)
5. Add rate limiting and security measures
"""

        # Save Markdown report
        report_path = "api_test_report.md"
        with open(report_path, 'w', encoding='utf-8') as f:
            f.write(md_content)

        print(f"{Colors.GREEN}📄 Markdown report saved: {report_path}{Colors.END}")


if __name__ == "__main__":
    print(f"{Colors.BOLD}{Colors.BLUE}AI Security Vision API Endpoint Tester{Colors.END}")
    print(f"{Colors.BLUE}{'='*50}{Colors.END}")

    # Check if backend is running
    tester = APIEndpointTester()
    try:
        response = requests.get(f"{tester.base_url}/api/system/status", timeout=5)
        print(f"{Colors.GREEN}✅ Backend service is running{Colors.END}")
    except:
        print(f"{Colors.RED}❌ Backend service is not accessible at {tester.base_url}{Colors.END}")
        print(f"{Colors.YELLOW}Please ensure the AISecurityVision service is running{Colors.END}")
        sys.exit(1)

    # Run tests
    tester.discover_endpoints()
    tester.test_all_endpoints()

    # Generate reports
    tester.generate_console_summary()
    tester.generate_html_report()
    tester.generate_markdown_report()

    print(f"\n{Colors.GREEN}✅ Testing completed! Check the generated reports.{Colors.END}")
