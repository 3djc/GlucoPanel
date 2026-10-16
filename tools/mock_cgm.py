#!/usr/bin/env python3
"""Mock Nightscout / Dexcom Share / LibreLinkUp server for the simulator.

    python3 tools/mock_cgm.py            # listens on localhost:9000
    SIM_API_BASE=http://localhost:9000 pio run -e sim -t exec

Credentials: Dexcom user/pass, LibreLinkUp a@b.c/pass ("redirect@test" exercises the region
redirect), Nightscout token "tok" (or none). Glucose follows a slow wave with a changing trend.
"""
import hashlib, json, math, os, sys, time
from datetime import datetime, timezone
from http.server import BaseHTTPRequestHandler, HTTPServer
from urllib.parse import parse_qs, urlparse

LIBRE_USER_ID = "11111111-2222-3333-4444-555555555555"
TOKEN = "libre-token-123"
SESSION = "dex-session-1"
TRENDS = ["DoubleDown", "SingleDown", "FortyFiveDown", "Flat", "FortyFiveUp", "SingleUp", "DoubleUp"]

START = time.time()

def at(t):
    """Glucose value and trend index at time t (a slow wave)."""
    phase = t / 90.0
    mgdl = round(130 + 70 * math.sin(phase))
    slope = math.cos(phase)  # -1..1
    idx = min(6, max(0, round((slope + 1) / 2 * 6)))
    return mgdl, idx

def reading():
    # measured two minutes ago; MOCK_FREEZE=1 keeps one measurement time so its age keeps growing
    ts = START - 100 if os.environ.get("MOCK_FREEZE") else time.time() - 120
    return at(ts) + (ts,)

def libre_stamp(ts):
    d = datetime.fromtimestamp(ts, timezone.utc)
    return "%d/%d/%d %d:%02d:%02d %s" % (d.month, d.day, d.year, d.hour % 12 or 12, d.minute, d.second,
                                          "PM" if d.hour >= 12 else "AM")

class H(BaseHTTPRequestHandler):
    def log_message(self, fmt, *a):
        sys.stderr.write("mock: %s\n" % (fmt % a))

    def reply(self, code, obj):
        raw = json.dumps(obj).encode()
        self.send_response(code)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(raw)))
        self.end_headers()
        self.wfile.write(raw)

    def body(self):
        n = int(self.headers.get("Content-Length") or 0)
        return json.loads(self.rfile.read(n) or b"{}") if n else {}

    def do_GET(self):
        u = urlparse(self.path)
        q = parse_qs(u.query)
        mgdl, idx, ts = reading()
        if u.path == "/api/v1/entries/sgv.json":
            if q.get("token", ["tok"])[0] != "tok":
                return self.reply(401, {"status": 401, "message": "Unauthorized"})
            n = min(int(q.get("count", ["1"])[0]), 40)
            out = []
            for i in range(n):
                v, k = at(ts - 300 * i)
                out.append({"sgv": v, "direction": TRENDS[k], "date": int((ts - 300 * i) * 1000)})
            return self.reply(200, out)
        if u.path == "/llu/connections":
            want = hashlib.sha256(LIBRE_USER_ID.encode()).hexdigest()
            if (self.headers.get("Authorization") != "Bearer " + TOKEN or self.headers.get("account-id") != want
                    or self.headers.get("product") != "llu.android" or not self.headers.get("version")):
                return self.reply(401, {"status": 401})
            arrow = {0: 1, 1: 1, 2: 2, 3: 3, 4: 4, 5: 5, 6: 5}[idx]
            return self.reply(200, {"status": 0, "data": [{"patientId": "p1", "glucoseMeasurement": {
                "ValueInMgPerDl": mgdl, "TrendArrow": arrow, "FactoryTimestamp": libre_stamp(ts)}}]})
        if u.path == "/llu/connections/p1/graph":
            want = hashlib.sha256(LIBRE_USER_ID.encode()).hexdigest()
            if self.headers.get("Authorization") != "Bearer " + TOKEN or self.headers.get("account-id") != want:
                return self.reply(401, {"status": 401})
            # like the real graph: history points 15 min apart, newest first
            graph = [{"ValueInMgPerDl": at(ts - 900 * i)[0], "FactoryTimestamp": libre_stamp(ts - 900 * i)} for i in range(1, 9)]
            return self.reply(200, {"status": 0, "data": {"connection": {}, "graphData": graph}})
        self.reply(404, {"error": "not found"})

    def do_POST(self):
        u = urlparse(self.path)
        q = parse_qs(u.query)
        b = self.body()
        host = self.headers.get("X-Orig-Host", "")
        if u.path == "/General/AuthenticatePublisherAccount" or u.path.endswith("/General/AuthenticatePublisherAccount"):
            if b.get("accountName") == "user" and b.get("password") == "pass":
                return self.reply(200, "aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee")
            return self.reply(500, {"Code": "AccountPasswordInvalid", "Message": "bad credentials"})
        if u.path.endswith("/General/LoginPublisherAccountById"):
            if b.get("accountId", "").startswith("aaaaaaaa") and b.get("password") == "pass":
                return self.reply(200, SESSION)
            return self.reply(500, {"Code": "AccountPasswordInvalid"})
        if u.path.endswith("/Publisher/ReadPublisherLatestGlucoseValues"):
            if q.get("sessionId", [""])[0] != SESSION:
                return self.reply(500, {"Code": "SessionNotValid", "Message": "session"})
            _, _, ts = reading()
            n = min(int(q.get("maxCount", ["1"])[0]), 8)
            out = []
            for i in range(n):
                t = ts - 300 * i
                mgdl, idx = at(t)
                dexnum = {0: 7, 1: 6, 2: 5, 3: 4, 4: 3, 5: 2, 6: 1}[idx]
                ms = int(t * 1000)
                out.append({"WT": "Date(%d)" % ms, "ST": "Date(%d)" % ms, "DT": "Date(%d-0500)" % ms, "Value": mgdl,
                            "Trend": TRENDS[idx] if int(t) % 2 else dexnum})  # alternate string / numeric trend forms
            return self.reply(200, out)
        if u.path == "/llu/auth/login":
            if b.get("email") == "redirect@test" and "api-de" not in host:
                return self.reply(200, {"status": 0, "data": {"redirect": True, "region": "de"}})
            if b.get("email") in ("a@b.c", "redirect@test") and b.get("password") == "pass":
                return self.reply(200, {"status": 0, "data": {"user": {"id": LIBRE_USER_ID},
                                                               "authTicket": {"token": TOKEN, "expires": 9999999999, "duration": 3600}}})
            return self.reply(200, {"status": 2, "error": {"message": "incorrect"}})
        self.reply(404, {"error": "not found"})

HTTPServer(("127.0.0.1", 9000), H).serve_forever()
