#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
"""test_spotgate.py - spotgate against a stand-in for go-librespot's API on
127.0.0.1 (made up here: two tracks of a playlist, tones of its own written
to a FIFO as the real player writes its sound): a playlist streamed as MP3
with each track's block (title, cover, EPlayTrack/Len/Pos) and its end;
Next and Previous inside it (from, step), a place to start (pos); /tracks;
/login, signed in and waiting for a code; what it refuses (another item
kind, a second listener, no player). Needs ffmpeg: SKIPPED where none.

    python3 radio/test_spotgate.py
"""
import json
import os
import socket
import struct
import sys
import tempfile
import threading
import time
import math
import unittest
import urllib.parse
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import spotgate  # noqa: E402

HAVE_FFMPEG = spotgate.radiogate.ffmpeg_version() is not None
TMP = tempfile.mkdtemp(prefix="spotgate-")
PL = "spotify:playlist:1AbCdEfGhIjKlMnOpQrStU"
T1 = "spotify:track:0000000000000000000001"
T2 = "spotify:track:0000000000000000000002"
TRACK_S = 2.0                       # each stand-in track: 2 s of a tone
TONES = None                        # (left Hz, right Hz) for every track; None: 440 Hz times its number


class Player:
    """go-librespot's API as far as spotgate uses it"""

    def __init__(self):
        self.fifo = os.path.join(TMP, "spotify.pcm")
        if not os.path.exists(self.fifo):
            os.mkfifo(self.fifo)
        self.code = None            # /auth/code's answer when waiting for one
        self.mute = False           # answers play, but no sound comes (a stuck player)
        self.gen = 0                # (one player: a new play ends the last one's writing)
        self.plays = []
        self.state = {"stopped": True, "paused": False, "track": None}
        self.paused = threading.Event()
        self.tracks = {T1: ("One", ["A Band"], "http://cover.example/1.jpg"),
                       T2: ("Two", ["A Band", "A Guest"], "http://cover.example/2.jpg")}
        me = self

        class H(BaseHTTPRequestHandler):
            def log_message(self, *a):
                pass

            def answer(self, status, obj=None):
                b = json.dumps(obj).encode() if obj is not None else b""
                self.send_response(status)
                self.send_header("Content-Length", str(len(b)))
                self.end_headers()
                self.wfile.write(b)

            def do_GET(self):
                u = urllib.parse.urlsplit(self.path)
                q = dict(urllib.parse.parse_qsl(u.query))
                if u.path == "/status":
                    return self.answer(200, dict(me.state, username="someone", context_name="Night Mix"))
                if u.path == "/auth/code":
                    return self.answer(200, me.code) if me.code else self.answer(404)
                if u.path == "/context/tracks" and q.get("uri") == PL:
                    return self.answer(200, {"uri": PL, "ready": True, "length": 2, "tracks": [
                        {"uri": t, "track": me.obj(t, 0)} for t in (T1, T2)]})
                self.answer(400)

            def do_POST(self):
                n = int(self.headers.get("Content-Length") or 0)
                body = json.loads(self.rfile.read(n) or b"{}")
                if self.path == "/player/play":
                    me.plays.append(body)
                    me.gen += 1
                    me.paused.clear()
                    if not me.mute:
                        threading.Thread(target=me.play, args=(body,), daemon=True).start()
                    else:
                        me.state.update(stopped=False, paused=False, track=me.obj(T1, 0))
                    return self.answer(200)
                if self.path == "/player/pause":
                    me.paused.set()
                    me.state["paused"] = True
                    return self.answer(200)
                self.answer(400)

        self.srv = ThreadingHTTPServer(("127.0.0.1", 0), H)
        self.port = self.srv.server_address[1]
        threading.Thread(target=self.srv.serve_forever, daemon=True).start()

    def obj(self, t, pos):
        name, who, cover = self.tracks[t]
        return {"uri": t, "name": name, "artist_names": who, "album_cover_url": cover,
                "duration": int(TRACK_S * 1000), "position": pos}

    def play(self, body):
        gen = self.gen
        order = [T1, T2] if body["uri"] == PL else [body["uri"]]
        start = order.index(body["skip_to_uri"]) if body.get("skip_to_uri") else 0
        pos = body.get("position", 0)
        self.state.update(stopped=False, paused=False)
        try:
            with open(self.fifo, "wb") as f:        # (as wait_for_reader: until spotgate reads)
                for i, t in enumerate(order[start:]):
                    self.state["track"] = self.obj(t, pos)
                    hl, hr = TONES or (440 * (i + start + 1),) * 2
                    n = int(44100 * (TRACK_S - pos / 1000))
                    for k in range(0, n, 4410):
                        if self.paused.is_set() or gen != self.gen:
                            return
                        f.write(b"".join(struct.pack("<hh", int(8000 * math.sin(2 * math.pi * hl * j / 44100)),
                                                     int(8000 * math.sin(2 * math.pi * hr * j / 44100)))
                                         for j in range(k, min(n, k + 4410))))
                        time.sleep(0.05)    # (faster than real time, as a player that is ahead)
                    pos = 0
        except (BrokenPipeError, OSError):
            return
        finally:
            if not self.paused.is_set() and gen == self.gen:
                self.state.update(stopped=True, track=None)

    def stop(self):
        self.srv.shutdown()


class Gate:
    def __init__(self):
        self.srv = ThreadingHTTPServer(("127.0.0.1", 0), spotgate.Handler)
        self.srv.daemon_threads = True
        self.port = self.srv.server_address[1]
        threading.Thread(target=self.srv.serve_forever, daemon=True).start()

    def ask(self, path, meta=True, want=10 ** 7, timeout=30):
        s = socket.create_connection(("127.0.0.1", self.port), timeout=timeout)
        s.sendall(("GET %s HTTP/1.0\r\n%s\r\n" % (path, "Icy-MetaData: 1\r\n" if meta else "")).encode())
        got = b""
        while len(got) < want:
            d = s.recv(65536)
            if not d:
                break
            got += d
        s.close()
        head, _, body = got.partition(b"\r\n\r\n")
        lines = head.decode("latin-1").split("\r\n")
        heads = dict((k.strip().lower(), v.strip()) for k, _, v in (l.partition(":") for l in lines[1:]))
        return lines[0], heads, body


def blocks(body, metaint=spotgate.METAINT):
    """(audio bytes, [each non-empty block's text])"""
    audio, out, i = bytearray(), [], 0
    while i < len(body):
        audio += body[i:i + metaint]
        i += metaint
        if i >= len(body):
            break
        n = body[i] * 16
        if n:
            out.append(body[i + 1:i + 1 + n].rstrip(b"\0").decode())
        i += 1 + n
    return bytes(audio), out


@unittest.skipUnless(HAVE_FFMPEG, "SKIPPED: no ffmpeg")
class Tests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.p = Player()
        spotgate.Config.api = "http://127.0.0.1:%d" % cls.p.port
        spotgate.Config.fifo = cls.p.fifo
        spotgate.Config.poll = 0.2
        spotgate.Config.start_s = 0.5
        cls.g = Gate()

    @classmethod
    def tearDownClass(cls):
        cls.g.srv.shutdown()
        cls.p.stop()

    def test_playlist(self):
        st, h, body = self.g.ask("/stream?uri=" + PL)
        self.assertIn("200", st)
        self.assertEqual(h.get("content-type"), "audio/mpeg")
        self.assertEqual(h.get("icy-metaint"), str(spotgate.METAINT))
        audio, bl = blocks(body)
        self.assertGreater(len(audio), 40000, "the two tracks' MP3")
        self.assertTrue(audio[:3] == b"ID3" or audio[0] == 0xff, "MP3 from its start")
        self.assertEqual(len(bl), 2, bl)
        self.assertEqual(bl[0], "StreamTitle='A Band - One';StreamUrl='http://cover.example/1.jpg';"
                         "EPlayTrack='%s';EPlayLen='2000';EPlayPos='0';" % T1)
        self.assertIn("StreamTitle='A Band, A Guest - Two';", bl[1])
        self.assertIn("EPlayTrack='%s'" % T2, bl[1])
        self.assertEqual(self.p.plays[-1], {"uri": PL})
        self.assertTrue(self.p.paused.is_set(), "the player paused when the stream ended")

    def test_next_previous_pos(self):
        self.g.ask("/stream?uri=%s&from=%s&step=1" % (PL, T1), want=20000)
        self.assertEqual(self.p.plays[-1], {"uri": PL, "skip_to_uri": T2})
        self.g.ask("/stream?uri=%s&from=%s&step=-1" % (PL, T2), want=20000)
        self.assertEqual(self.p.plays[-1], {"uri": PL, "skip_to_uri": T1})
        st, _, body = self.g.ask("/stream?uri=%s&from=%s&pos=1500" % (PL, T2))
        self.assertEqual(self.p.plays[-1], {"uri": PL, "skip_to_uri": T2, "position": 1500})
        _, bl = blocks(body)
        self.assertIn("EPlayPos='1500'", bl[0])
        st, _, _ = self.g.ask("/stream?uri=%s&from=%s&step=1" % (PL, T2))
        self.assertIn("404", st, "no track after the last")

    def test_tracks(self):
        st, h, body = self.g.ask("/tracks?uri=" + PL, meta=False)
        self.assertIn("200", st)
        self.assertEqual(body.decode(), "2000\tA Band\tOne\t%s\n2000\tA Band, A Guest\tTwo\t%s\n" % (T1, T2))
        st, _, _ = self.g.ask("/tracks?uri=" + T1, meta=False)
        self.assertIn("400", st)

    def test_login(self):
        self.p.code = None
        self.assertEqual(self.g.ask("/login", meta=False)[2], b"ready\tsomeone\n")
        self.p.code = {"url": "https://www.spotify.com/pair", "code": "ABCD12"}
        try:
            self.assertEqual(self.g.ask("/login", meta=False)[2], b"code\thttps://www.spotify.com/pair\tABCD12\n")
        finally:
            self.p.code = None

    def test_refused(self):
        for bad in ("spotify:artist:0OdUWJ0sBjDrqHygGUXeCF", "spotify:track:short", "http://x/y.mp3"):
            st, _, body = self.g.ask("/stream?uri=" + urllib.parse.quote(bad), meta=False)
            self.assertIn("400", st, bad)
            self.assertTrue(body.startswith(b"spotgate: "))
        st, _, _ = self.g.ask("/stream?uri=%s&pos=x" % PL)
        self.assertIn("400", st)

    def test_one_at_a_time(self):
        spotgate.Config.busy_s = 0.2
        self.addCleanup(setattr, spotgate.Config, "busy_s", 3.0)
        first = threading.Thread(target=self.g.ask, args=("/stream?uri=" + PL,))
        first.start()
        time.sleep(0.6)
        st, _, body = self.g.ask("/stream?uri=" + PL)
        first.join()
        self.assertIn("503", st)
        self.assertIn(b"another Atari", body)

    def test_stuck_player(self):
        # (the player says it plays, and no sound comes: 502, and the one
        # stream free again for the next)
        spotgate.Config.first_s = 1.5
        self.p.mute = True
        try:
            t0 = time.time()
            st, _, body = self.g.ask("/stream?uri=" + PL)
            self.assertIn("502", st)
            self.assertIn(b"no sound", body)
            self.assertLess(time.time() - t0, 6)
        finally:
            self.p.mute = False
            spotgate.Config.first_s = 15
        st, _, body = self.g.ask("/stream?uri=" + PL)
        self.assertIn("200", st)

    def test_no_proxy(self):
        # (HTTP_PROXY set, as on a Pi behind a proxy: the player still asked directly)
        os.environ["HTTP_PROXY"] = os.environ["http_proxy"] = "http://127.0.0.1:9"
        try:
            self.assertEqual(self.g.ask("/login", meta=False)[2], b"ready\tsomeone\n")
        finally:
            del os.environ["HTTP_PROXY"], os.environ["http_proxy"]

    def test_no_player(self):
        keep = spotgate.Config.api
        spotgate.Config.api = "http://127.0.0.1:9"
        try:
            st, _, body = self.g.ask("/login", meta=False)
            self.assertIn("503", st)
            self.assertIn(b"isn't running", body)
        finally:
            spotgate.Config.api = keep


if __name__ == "__main__":
    unittest.main(verbosity=2)
