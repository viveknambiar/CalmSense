from http.server import ThreadingHTTPServer, SimpleHTTPRequestHandler
import json
import os
import time

BASE_DIR = os.path.dirname(os.path.abspath(__file__))
LOG_FILE = os.path.join(BASE_DIR, "vitals_stream.jsonl")
WEB_DIR = os.path.join(BASE_DIR, "web")

FILE_OFFSET = 0
LOGGED_FIRST_READ = False
LAST_GOOD_RECORD = {
    "timestamp": 0,
    "pulse": 0,
    "breathing": 0,
    "rmssd": 0,
    "stress_index": 0,
    "blink_rate": 0,
    "hands_to_face": False,
    "stage": 0
}

def read_next_vitals():
    global FILE_OFFSET, LAST_GOOD_RECORD, LOGGED_FIRST_READ
    if not os.path.exists(LOG_FILE):
        return LAST_GOOD_RECORD

    try:
        file_size = os.path.getsize(LOG_FILE)
        # If file was truncated/restarted by C++, reset offset to 0
        if file_size < FILE_OFFSET:
            FILE_OFFSET = 0

        with open(LOG_FILE, "r", encoding="utf-8") as f:
            f.seek(FILE_OFFSET)
            line = f.readline()
            # Only advance offset if we read a complete newline-terminated JSON line
            if line and line.endswith("\n"):
                FILE_OFFSET = f.tell()
                record = json.loads(line.strip())
                record["timestamp"] = int(time.time() * 1000)
                LAST_GOOD_RECORD = record
                if not LOGGED_FIRST_READ:
                    print(f"[Python Server] Reading vitals from {LOG_FILE} -> Pulse: {record.get('pulse')} BPM", flush=True)
                    LOGGED_FIRST_READ = True
    except Exception as e:
        print(f"[Read Warning] {e}", flush=True)

    return LAST_GOOD_RECORD

class CalmSenseHandler(SimpleHTTPRequestHandler):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=WEB_DIR, **kwargs)

    def do_GET(self):
        if self.path.startswith("/vitals.json") or self.path.startswith("/api/vitals"):
            payload = read_next_vitals()
            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.send_header("Cache-Control", "no-store")
            self.end_headers()
            self.wfile.write(json.dumps(payload).encode("utf-8"))
        else:
            super().do_GET()

    def log_message(self, format, *args):
        if "GET /vitals.json" not in format % args and "GET /api/vitals" not in format % args:
            super().log_message(format, *args)

if __name__ == "__main__":
    print(f"Watching JSONL file: {LOG_FILE}", flush=True)
    server = ThreadingHTTPServer(("0.0.0.0", 8080), CalmSenseHandler)
    print("CalmSense Server running on http://localhost:8080 ...", flush=True)
    server.serve_forever()
