#!/usr/bin/env python3
import http.server
import socketserver
import json
import sys
from datetime import datetime

class AlarmHandler(http.server.BaseHTTPRequestHandler):
    def do_POST(self):
        content_length = int(self.headers.get('Content-Length', 0))
        post_data = self.rfile.read(content_length)

        try:
            alarm_data = json.loads(post_data.decode('utf-8'))
            timestamp = datetime.now().isoformat()

            print(f"[{timestamp}] Received alarm:")
            print(f"  Event Type: {alarm_data.get('event_type', 'N/A')}")
            print(f"  Camera ID: {alarm_data.get('camera_id', 'N/A')}")
            print(f"  Confidence: {alarm_data.get('confidence', 'N/A')}")
            print(f"  Test Mode: {alarm_data.get('test_mode', False)}")
            print(f"  Timestamp: {alarm_data.get('timestamp', 'N/A')}")
            print(f"  Bounding Box: {alarm_data.get('bounding_box', 'N/A')}")
            print("  Raw JSON:", json.dumps(alarm_data, indent=2))
            print("-" * 50)

            # Log to file for verification
            with open('received_alarms.log', 'a') as f:
                f.write(f"{timestamp}: {json.dumps(alarm_data)}\n")

            self.send_response(200)
            self.send_header('Content-type', 'application/json')
            self.end_headers()
            self.wfile.write(b'{"status": "alarm_received", "timestamp": "' + timestamp.encode() + b'"}')

        except Exception as e:
            print(f"Error processing alarm: {e}")
            self.send_response(400)
            self.end_headers()
            self.wfile.write(b'{"error": "Invalid JSON"}')

    def log_message(self, format, *args):
        pass  # Suppress default logging

if __name__ == "__main__":
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 8081
    with socketserver.TCPServer(("", port), AlarmHandler) as httpd:
        print(f"Test alarm server listening on port {port}")
        httpd.serve_forever()
