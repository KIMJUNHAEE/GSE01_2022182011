"""Receiver-only tests. Never launches or builds the game."""
import copy
import http.client
import json
import threading
import unittest

from server import MAX_BODY, Server, State, unique_object, validate_snapshot


def snapshot(session="test-session", sequence="1"):
    return {
        "schemaVersion": 1, "type": "world_snapshot", "sessionId": session, "sequence": sequence,
        "eventsTotal": "1", "mapSeed": 42, "gameTime": 2.5, "view": "explore",
        "counts": {"npcs": 1, "npcsIncludingInactive": 1, "mobs": 0, "mobsIncludingInactive": 0,
                   "projectiles": 0, "loot": 0, "actors": 5, "unassignedNpcs": 0, "unassignedMobs": 0},
        "player": {"level": 1, "health": 100, "maxHealth": 100, "experience": 0, "weaponTier": 0,
                   "kills": 0, "itemsCollected": 0, "shotsFired": 0, "active": True,
                   "position": {"x": 0, "y": 0}},
        "progress": {"quest": 1, "restoredRelays": 0, "memoriesFound": 0, "bossActive": False,
                     "bossDefeated": False, "objective": "주민을 도우세요", "region": "테스트 구역"},
        "performance": {"ready": True, "fps": 60.5, "averageDrawCalls": 33.5,
                        "lastDrawCalls": 34, "averageInstancesPerDraw": 2.3},
        "villages": [{"id": 0, "name": "테스트 구역", "alignment": "pirate", "mood": "quiet",
                      "alignmentSource": "designer_metadata", "moodSource": "test",
                      "npcCount": 1, "mobCount": 0, "relayRestored": None,
                      "bounds": {"x0": -1, "y0": -1, "x1": 1, "y1": 1}}],
        "npcs": [{"id": "8", "name": "주민", "role": "resident", "state": "idle", "active": True,
                  "visible": True, "villageId": 0, "position": {"x": .5, "y": .5}, "health": None}],
        "mobs": [],
        "events": [{"id": "1", "gameTime": 0, "kind": "world_created", "severity": "info",
                    "message": "한글 \"따옴표\"\n줄바꿈", "villageId": -1, "subjectId": "0",
                    "confirmed": True, "source": "game"}],
    }


def report(session="test-session"):
    return {"schemaVersion": 1, "type": "world_event", "sessionId": session,
            "kind": "murder_rumor", "severity": "warning", "villageId": 0,
            "message": "<img src=x onerror=alert(1)> 미확인 소문"}


class StateTests(unittest.TestCase):
    def test_snapshot_unicode_and_counts(self):
        data = snapshot()
        self.assertEqual(validate_snapshot(data)["npcs"][0]["name"], "주민")
        state = State()
        state.publish(data)
        self.assertFalse(state.read()["stale"])
        self.assertEqual(state.read()["snapshot"]["counts"]["npcs"], 1)

    def test_stale_and_waiting(self):
        state = State()
        self.assertTrue(state.read()["stale"])
        state.publish(snapshot())
        state.received_clock -= 4
        self.assertTrue(state.read()["stale"])
        self.assertIsNotNone(state.read()["snapshot"])

    def test_restart_and_late_messages(self):
        state = State()
        state.publish(snapshot(sequence="10"))
        with self.assertRaises(ValueError):
            state.publish(snapshot(sequence="9"))
        state.report(report())
        state.publish(snapshot("new-session"))
        self.assertFalse(state.read()["externalEvents"])
        self.assertEqual(len(state.read()["history"]), 1)
        with self.assertRaises(ValueError):
            state.publish(snapshot(sequence="11"))

    def test_external_reports_do_not_mutate_game_or_claim_confirmation(self):
        state = State()
        original = snapshot()
        saved = copy.deepcopy(original)
        state.publish(original)
        data = report()
        data["confirmed"] = True
        data["source"] = "game"
        result = state.report(data)
        self.assertFalse(result["confirmed"])
        self.assertEqual(result["source"], "external")
        self.assertEqual(state.read()["snapshot"], saved)
        with self.assertRaises(ValueError):
            state.report(report("wrong-session"))

    def test_bounded_buffers(self):
        state = State()
        for i in range(150):
            state.publish(snapshot(sequence=str(i + 1)))
            state.report(report())
        self.assertEqual(len(state.read()["history"]), 120)
        self.assertEqual(len(state.read()["externalEvents"]), 128)

    def test_invalid_schema(self):
        for mutate in (
            lambda d: d.update(schemaVersion=2),
            lambda d: d["performance"].update(fps=float("nan")),
            lambda d: d.update(gameTime=10 ** 1000),
            lambda d: d["npcs"][0].update(name="\ud800"),
            lambda d: d["counts"].update(mobs=-1),
            lambda d: d["counts"].update(mobs=True),
            lambda d: d["villages"].append(copy.deepcopy(d["villages"][0])),
            lambda d: d["npcs"][0].update(villageId=88),
            lambda d: d.update(sequence=1),
        ):
            data = snapshot()
            mutate(data)
            with self.assertRaises(ValueError):
                validate_snapshot(data)
        with self.assertRaises(ValueError):
            json.loads('{"a":1,"a":2}', object_pairs_hook=unique_object)


class HttpTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.server = Server(("127.0.0.1", 0))
        cls.worker = threading.Thread(target=cls.server.serve_forever, daemon=True)
        cls.worker.start()

    @classmethod
    def tearDownClass(cls):
        cls.server.shutdown()
        cls.server.server_close()
        cls.worker.join(timeout=3)

    def request(self, method, path, data=None, headers=None):
        connection = http.client.HTTPConnection("127.0.0.1", self.server.server_port, timeout=3)
        try:
            body = None if data is None else json.dumps(data, ensure_ascii=False).encode("utf-8")
            request_headers = {"Content-Type": "application/json"}
            request_headers.update(headers or {})
            connection.request(method, path, body=body, headers=request_headers)
            response = connection.getresponse()
            return response.status, response.getheaders(), response.read()
        finally:
            connection.close()

    def test_http_roundtrip(self):
        status, _, _ = self.request("POST", "/api/snapshot", snapshot("http-session"))
        self.assertEqual(status, 202)
        status, _, body = self.request("GET", "/api/state")
        self.assertEqual(status, 200)
        self.assertEqual(json.loads(body)["snapshot"]["sessionId"], "http-session")
        status, _, _ = self.request("POST", "/api/events", report("http-session"))
        self.assertEqual(status, 202)

    def test_html_and_static_allowlist(self):
        for path in ("/", "/styles.css", "/dashboard.js"):
            status, headers, _ = self.request("GET", path)
            self.assertEqual(status, 200)
            self.assertIn("Content-Security-Policy", dict(headers))
        self.assertEqual(self.request("GET", "/../server.py")[0], 404)
        self.assertEqual(self.request("GET", "/server.py")[0], 404)

    def test_origin_host_and_payload_guards(self):
        self.assertEqual(self.request("POST", "/api/snapshot", snapshot(),
                                     {"Origin": "https://evil.example"})[0], 403)
        self.assertEqual(self.request("GET", "/", headers={"Host": "evil.example"})[0], 403)
        self.assertEqual(self.request("POST", "/api/events", {}, {"Content-Type": "text/plain"})[0], 415)
        self.assertEqual(self.request("POST", "/api/events", {}, {"Content-Length": str(MAX_BODY + 1)})[0], 413)
        self.assertEqual(self.request("OPTIONS", "/api/events")[0], 403)


if __name__ == "__main__":
    unittest.main()
