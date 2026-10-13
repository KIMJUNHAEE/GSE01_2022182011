"""Loopback-only JSON receiver and read-only game dashboard. Python 3.9+; stdlib only."""
import argparse
from collections import deque
from datetime import datetime, timezone
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import json
import math
from pathlib import Path
import threading
import time
from urllib.parse import urlsplit
import uuid

MAX_BODY = 2 * 1024 * 1024
STATIC = Path(__file__).resolve().parent
SEVERITIES = {"info", "warning", "danger"}
EXTERNAL_KINDS = {"murder_rumor", "theft_rumor", "disturbance", "observation"}


def utc_now():
    return datetime.now(timezone.utc).isoformat()


def text(value, label, limit=256):
    if not isinstance(value, str) or not value or len(value) > limit:
        raise ValueError(f"{label}: expected 1..{limit} characters")
    value.encode("utf-8")  # reject unpaired JSON Unicode surrogates
    return value


def number(value, label, minimum=-1e12, maximum=1e12):
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        raise ValueError(f"{label}: expected finite number")
    if not minimum <= value <= maximum or not math.isfinite(value):
        raise ValueError(f"{label}: out of range")
    return value


def integer(value, label, minimum=-1, maximum=1000000):
    if type(value) is not int or not minimum <= value <= maximum:
        raise ValueError(f"{label}: expected integer")
    return value


def flag(value, label):
    if type(value) is not bool:
        raise ValueError(f"{label}: expected boolean")


def identifier(value, label):
    text(value, label, 20)
    if not value.isascii() or not value.isdecimal():
        raise ValueError(f"{label}: expected decimal string")
    return int(value)


def obj(value, label):
    if not isinstance(value, dict):
        raise ValueError(f"{label}: expected object")
    return value


def array(value, label, limit):
    if not isinstance(value, list) or len(value) > limit:
        raise ValueError(f"{label}: expected array of at most {limit}")
    return value


def position(value):
    value = obj(value, "position")
    number(value.get("x"), "position.x")
    number(value.get("y"), "position.y")


def validate_snapshot(data):
    obj(data, "message")
    if type(data.get("schemaVersion")) is not int or data["schemaVersion"] != 1 or data.get("type") != "world_snapshot":
        raise ValueError("Expected schemaVersion=1, type=world_snapshot")
    text(data.get("sessionId"), "sessionId", 128)
    identifier(data.get("sequence"), "sequence")
    identifier(data.get("eventsTotal"), "eventsTotal")
    number(data.get("gameTime"), "gameTime", 0)
    integer(data.get("mapSeed"), "mapSeed", 0, 4294967295)
    if data.get("view") not in {"title", "explore", "dialogue", "map", "journal", "pause", "ending", "defeat", "unknown"}:
        raise ValueError("Unknown game view")
    counts = obj(data.get("counts"), "counts")
    for key in ("npcs", "npcsIncludingInactive", "mobs", "mobsIncludingInactive", "projectiles",
                "loot", "actors", "unassignedNpcs", "unassignedMobs"):
        integer(counts.get(key), f"counts.{key}", 0)
    player = obj(data.get("player"), "player")
    for key in ("level", "health", "maxHealth", "experience", "weaponTier", "kills", "itemsCollected", "shotsFired"):
        number(player.get(key), f"player.{key}", 0)
    flag(player.get("active"), "player.active")
    position(player.get("position"))
    progress = obj(data.get("progress"), "progress")
    for key in ("quest", "restoredRelays", "memoriesFound"):
        integer(progress.get(key), f"progress.{key}", 0)
    for key in ("bossActive", "bossDefeated"):
        flag(progress.get(key), f"progress.{key}")
    for key in ("objective", "region"):
        text(progress.get(key), f"progress.{key}", 1000)
    perf = obj(data.get("performance"), "performance")
    flag(perf.get("ready"), "performance.ready")
    for key in ("fps", "averageDrawCalls", "lastDrawCalls", "averageInstancesPerDraw"):
        if perf.get(key) is not None:
            number(perf[key], f"performance.{key}", 0)
        elif perf["ready"] or key == "lastDrawCalls":
            raise ValueError(f"performance.{key} is missing")

    villages = array(data.get("villages"), "villages", 100)
    village_ids = set()
    for village in villages:
        obj(village, "village")
        vid = integer(village.get("id"), "village.id", 0)
        if vid in village_ids:
            raise ValueError("Duplicate village id")
        village_ids.add(vid)
        text(village.get("name"), "village.name")
        if village.get("alignment") not in {"civilian", "pirate", "neutral", "unknown"}:
            raise ValueError("Unknown alignment")
        if village.get("mood") not in {"crisis", "threatened", "recovering", "quiet"}:
            raise ValueError("Unknown mood")
        text(village.get("alignmentSource"), "alignmentSource")
        text(village.get("moodSource"), "moodSource")
        for key in ("npcCount", "mobCount"):
            integer(village.get(key), key, 0)
        if village.get("relayRestored") is not None:
            flag(village["relayRestored"], "relayRestored")
        if village.get("bounds") is not None:
            bounds = obj(village["bounds"], "bounds")
            for key in ("x0", "y0", "x1", "y1"):
                number(bounds.get(key), key)
            if bounds["x1"] <= bounds["x0"] or bounds["y1"] <= bounds["y0"]:
                raise ValueError("Invalid village bounds")

    entity_ids = set()
    for group in ("npcs", "mobs"):
        for entity in array(data.get(group), group, 5000):
            obj(entity, "entity")
            eid = identifier(entity.get("id"), "entity.id")
            if eid in entity_ids:
                raise ValueError("Duplicate entity id")
            entity_ids.add(eid)
            for key in ("name", "role", "state"):
                text(entity.get(key), key)
            for key in ("active", "visible"):
                flag(entity.get(key), key)
            vid = integer(entity.get("villageId"), "entity.villageId")
            if vid != -1 and vid not in village_ids:
                raise ValueError("Unknown entity village")
            position(entity.get("position"))
            for key in ("health", "maxHealth"):
                if entity.get(key) is not None:
                    number(entity[key], key, 0)
    event_ids = set()
    for event in array(data.get("events"), "events", 256):
        obj(event, "event")
        eid = identifier(event.get("id"), "event.id")
        if eid in event_ids:
            raise ValueError("Duplicate event id")
        event_ids.add(eid)
        identifier(event.get("subjectId"), "subjectId")
        number(event.get("gameTime"), "event.gameTime", 0)
        integer(event.get("villageId"), "event.villageId")
        text(event.get("kind"), "kind", 64)
        text(event.get("message"), "message", 1000)
        flag(event.get("confirmed"), "confirmed")
        if event.get("source") != "game" or event.get("severity") not in SEVERITIES:
            raise ValueError("Invalid game event source/severity")
    return data


class State:
    def __init__(self):
        self.lock = threading.Lock()
        self.snapshot = None
        self.received_at = None
        self.received_clock = 0
        self.retired = deque(maxlen=64)
        self.external = deque(maxlen=128)
        self.history = deque(maxlen=120)

    def publish(self, data):
        validate_snapshot(data)
        with self.lock:
            session = data["sessionId"]
            if session in self.retired:
                raise ValueError("Retired game session")
            if self.snapshot:
                if session == self.snapshot["sessionId"]:
                    if int(data["sequence"]) <= int(self.snapshot["sequence"]):
                        raise ValueError("Out-of-order snapshot")
                else:
                    self.retired.append(self.snapshot["sessionId"])
                    self.external.clear()
                    self.history.clear()
            self.snapshot = data
            self.received_at = utc_now()
            self.received_clock = time.monotonic()
            self.history.append({
                "at": self.received_at, "fps": data["performance"]["fps"],
                "drawCalls": data["performance"]["averageDrawCalls"],
                "mobs": data["counts"]["mobs"],
            })

    def report(self, data):
        obj(data, "message")
        if type(data.get("schemaVersion")) is not int or data["schemaVersion"] != 1 or data.get("type") != "world_event":
            raise ValueError("Expected schemaVersion=1, type=world_event")
        if data.get("kind") not in EXTERNAL_KINDS or data.get("severity") not in SEVERITIES:
            raise ValueError("Unknown external event kind/severity")
        message = text(data.get("message"), "message", 500)
        village = integer(data.get("villageId"), "villageId")
        session = text(data.get("sessionId"), "sessionId", 128)
        with self.lock:
            if not self.snapshot or session != self.snapshot["sessionId"]:
                raise ValueError("Report must reference the current game session")
            if village != -1 and village not in {v["id"] for v in self.snapshot["villages"]}:
                raise ValueError("Unknown village")
            event = {
                "id": str(uuid.uuid4()), "gameTime": self.snapshot["gameTime"],
                "kind": data["kind"], "severity": data["severity"], "message": message,
                "villageId": village, "subjectId": "0", "source": "external",
                "confirmed": False, "receivedAt": utc_now(),
            }
            self.external.append(event)
            return event

    def read(self):
        with self.lock:
            age = time.monotonic() - self.received_clock if self.snapshot else None
            return {
                "schemaVersion": 1, "receivedAt": self.received_at,
                "ageSeconds": round(age, 2) if age is not None else None,
                "stale": age is None or age > 3,
                "snapshot": self.snapshot, "externalEvents": list(self.external),
                "history": list(self.history),
            }


def unique_object(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValueError(f"Duplicate JSON key: {key}")
        result[key] = value
    return result


class Server(ThreadingHTTPServer):
    daemon_threads = True

    def __init__(self, address):
        super().__init__(address, Handler)
        self.state = State()


class Handler(BaseHTTPRequestHandler):
    server_version = "StarDrifterMonitor/1"

    def setup(self):
        super().setup()
        self.connection.settimeout(3)

    def log_message(self, *_):
        pass  # polling must not flood the terminal

    def trusted_request(self):
        port = self.server.server_port
        hosts = {f"127.0.0.1:{port}", f"localhost:{port}"}
        origin = self.headers.get("Origin")
        return (self.headers.get("Host") in hosts and
                (origin is None or origin in {f"http://{host}" for host in hosts}))

    def respond(self, code, body, mime="application/json; charset=utf-8"):
        if not isinstance(body, bytes):
            body = json.dumps(body, ensure_ascii=False, allow_nan=False).encode("utf-8")
        self.send_response(code)
        self.send_header("Content-Type", mime)
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Cache-Control", "no-store")
        self.send_header("X-Content-Type-Options", "nosniff")
        self.send_header("Content-Security-Policy",
                         "default-src 'self'; script-src 'self'; style-src 'self'; "
                         "connect-src 'self'; img-src 'self'; frame-ancestors 'none'; base-uri 'none'")
        self.send_header("Connection", "close")
        self.end_headers()
        self.wfile.write(body)
        self.close_connection = True

    def do_GET(self):
        if not self.trusted_request():
            self.respond(403, {"error": "Loopback same-origin requests only"})
            return
        path = urlsplit(self.path).path
        if path == "/api/state":
            self.respond(200, self.server.state.read())
            return
        files = {"/": ("index.html", "text/html; charset=utf-8"),
                 "/index.html": ("index.html", "text/html; charset=utf-8"),
                 "/dashboard.js": ("dashboard.js", "text/javascript; charset=utf-8"),
                 "/styles.css": ("styles.css", "text/css; charset=utf-8")}
        if path not in files:
            self.respond(404, {"error": "Not found"})
            return
        filename, mime = files[path]
        self.respond(200, (STATIC / filename).read_bytes(), mime)

    def do_POST(self):
        if not self.trusted_request():
            self.respond(403, {"error": "Loopback same-origin requests only"})
            return
        if self.headers.get_content_type() != "application/json" or self.headers.get("Transfer-Encoding"):
            self.respond(415, {"error": "Send application/json with Content-Length"})
            return
        try:
            length = int(self.headers.get("Content-Length", "0"))
            if length <= 0 or length > MAX_BODY:
                self.respond(413, {"error": "JSON payload must be 1..2097152 bytes"})
                return
            body = self.rfile.read(length)
            if len(body) != length:
                raise ValueError("Incomplete body")
            data = json.loads(body.decode("utf-8"), object_pairs_hook=unique_object,
                              parse_constant=lambda value: (_ for _ in ()).throw(ValueError(f"Invalid {value}")))
            # Unknown extension fields must also remain serializable for GET /api/state.
            json.dumps(data, ensure_ascii=False, allow_nan=False).encode("utf-8")
            path = urlsplit(self.path).path
            if path == "/api/snapshot":
                self.server.state.publish(data)
                self.respond(202, {"accepted": True})
            elif path == "/api/events":
                self.respond(202, {"accepted": True, "event": self.server.state.report(data)})
            else:
                self.respond(404, {"error": "Not found"})
        except (ValueError, TypeError, KeyError, RecursionError, UnicodeError) as error:
            self.respond(400, {"error": str(error)[:300]})
        except (TimeoutError, OSError):
            self.close_connection = True

    def do_OPTIONS(self):
        self.respond(403, {"error": "Cross-origin access is disabled"})


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", type=int, default=8765)
    args = parser.parse_args()
    if not 1024 <= args.port <= 65535:
        parser.error("--port must be between 1024 and 65535")
    try:
        server = Server(("127.0.0.1", args.port))
    except OSError as error:
        parser.exit(1, f"Cannot start monitor: {error}\n")
    print(f"STAR DRIFTER monitor: http://127.0.0.1:{args.port}/", flush=True)
    print("Waiting for game JSON. Ctrl+C stops the receiver. No game commands are exposed.", flush=True)
    try:
        server.serve_forever(poll_interval=0.25)
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()


if __name__ == "__main__":
    main()
