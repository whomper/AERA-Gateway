#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
"""spotgate - Spotify for EPlay through the Gateway (port 8581).

A Spotify line in EPlay's list (spotify:track:ID, spotify:album:ID or
spotify:playlist:ID; decided 27 Nov 2026: a line is a pointer to the
item Spotify keeps, and plays as it is in Spotify at that moment) is
played here by go-librespot, the Spotify player on the Gateway, and sent
to the Atari as MP3 in Icecast's way, as a station is by radiogate. No
phone or other device controls it: EPlay does.

    GET /stream?uri=<spotify:KIND:ID>[&from=<spotify:track:ID>][&step=1|-1][&pos=<ms>] HTTP/1.0
    Icy-MetaData: 1

  answered by

    HTTP/1.0 200 OK
    Content-Type: audio/mpeg
    icy-name: <the playlist's or album's name>   icy-br: 192   icy-metaint: 8192

  and the item played from its start, or from the track `from` (the one
  after it with step=1, before it with step=-1: EPlay's Next and Previous
  inside the line), `pos` ms into it (Pause, then Play: EPlay closes the
  stream at Pause and asks again where it was). Each track's block
  carries, besides StreamTitle='artists - title' and StreamUrl='its album
  cover' (fetched as a station's picture, by radiogate's /art):

    EPlayTrack='spotify:track:ID';EPlayLen='ms';EPlayPos='ms';

  the track, its length and where it starts in it, so the bar shows the
  track's place. No sound within 15 s of the start, or for 30 s once
  begun: the stream ends (502 before its first byte), the player paused,
  so no stuck player holds the one stream. The stream ends where the item ends (EPlay goes on to the
  next line); one stream at a time (the Gateway has one Spotify player):
  a request waits up to 3 s for the last one to end (EPlay's Next asks as
  it closes it), else 503 while another Atari listens.

    GET /tracks?uri=<spotify:album:ID or spotify:playlist:ID>

  answered by its tracks in order (EPlay's Expand: one line a track, by
  the user's choice, never by itself), text/plain, UTF-8, one a line:
  "ms<TAB>artists<TAB>title<TAB>spotify:track:ID".

    GET /login

  "ready<TAB>account" when the player is signed in; "code<TAB>link<TAB>code"
  when it waits for one (Spotify's pairing: the link opened on any device,
  the code typed there; once, the player keeps its credentials); 503 when
  the player isn't running.

    GET /version   ->  "spotgate 0.1, ffmpeg 6.1"

Errors are one line "spotgate: why", as radiogate's: 400 an item EPlay
doesn't play here, 403 not from the home network, 502 the player refused
or failed, 503 busy or no player.

go-librespot (GPL-3.0) runs beside it with its API on 127.0.0.1 only and
its sound to a FIFO:

    server: {enabled: true, address: 127.0.0.1, port: 3678}
    audio_backend: pipe
    audio_output_pipe: /run/aera/spotify.pcm       (made with mkfifo)
    audio_output_pipe_format: s16le
    audio_output_pipe_wait_for_reader: true
    credentials: {type: device_auth}
    metadata: {enabled: true}

It needs Spotify Premium. The account's credentials stay with the player
on the Gateway, never in EPLAY.INF nor here. The log says who listened and
how long, never which item.

Standard Python 3 only, ffmpeg, and radiogate's ICY and ffmpeg helpers.
"""

import argparse
import json
import os
import re
import resource
import select
import shutil
import subprocess
import threading
import time
import urllib.error
import urllib.request
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import parse_qsl, quote, urlsplit

HERE = os.path.dirname(os.path.abspath(__file__))
for d in (HERE, os.path.join(HERE, "..", "gateway")):
    if os.path.exists(os.path.join(d, "aeragate.py")):
        import sys
        sys.path.insert(0, d)
        break
import aeragate  # noqa: E402
import radiogate  # noqa: E402

VERSION = "0.1"
KBPS = 192
METAINT = radiogate.METAINT
ITEM = re.compile(r"spotify:(track|album|playlist):[0-9A-Za-z]{22}\Z")
TRACK = re.compile(r"spotify:track:[0-9A-Za-z]{22}\Z")
LIST_MAX = 5000                     # /tracks: tracks at most


class Config:
    api = "http://127.0.0.1:3678"
    fifo = "/run/aera/spotify.pcm"
    allow = []
    ffmpeg = "ffmpeg"
    memory = 256 << 20
    poll = 1.0                      # the player's state asked every second
    ready_s = 20                    # /tracks: how long the player may take to list an item
    start_s = 15                    # /stream: the player's "stopped" not taken as the end before this
    busy_s = 3.0                    # /stream: how long it waits for the last stream to end
    first_s = 15                    # /stream: how long its first sound may take
    idle_s = 30                     # /stream: how long the sound may stop once begun
    verbose = False


class Refused(Exception):
    def __init__(self, status, words):
        super().__init__(words)
        self.status, self.words = status, words


ONE = threading.Lock()              # one stream at a time: one Spotify player
# (the player is on this computer: never through HTTP_PROXY or HTTPS_PROXY,
# which urllib's own opener follows, 127.0.0.1 included: the Gateway's review)
NOPROXY = urllib.request.build_opener(urllib.request.ProxyHandler({}))


def api(method, path, body=None, timeout=10):
    """(status, the JSON answer or None) from go-librespot's API; Refused
    503 when it doesn't answer"""
    data = json.dumps(body).encode() if body is not None else None
    req = urllib.request.Request(Config.api + path, data=data, method=method,
                                 headers={"Content-Type": "application/json"} if data else {})
    try:
        with NOPROXY.open(req, timeout=timeout) as r:
            raw = r.read(4 << 20)
            status = r.status
    except urllib.error.HTTPError as e:
        raw, status = e.read(4096), e.code
    except (OSError, ValueError):
        raise Refused(503, "the Spotify player isn't running on the Gateway")
    try:
        return status, json.loads(raw) if raw.strip() else None
    except ValueError:
        return status, None


def item_tracks(uri):
    """the item's tracks, in order: [(track uri, its track object or None)];
    a track alone is itself"""
    if TRACK.match(uri):
        return [(uri, None)]
    t0 = time.time()
    while True:
        st, js = api("GET", "/context/tracks?uri=" + quote(uri, safe=""), timeout=20)
        if st == 404:
            raise Refused(502, "the Spotify player has no metadata cache (metadata: enabled)")
        if st != 200 or not isinstance(js, dict):
            raise Refused(502, "the Spotify player couldn't list it (%d)" % st)
        if js.get("ready") or time.time() - t0 > Config.ready_s:
            out = []
            for t in js.get("tracks") or []:
                if isinstance(t, dict) and isinstance(t.get("uri"), str) and TRACK.match(t["uri"]):
                    out.append((t["uri"], t.get("track") if isinstance(t.get("track"), dict) else None))
            return out[:LIST_MAX]
        time.sleep(0.5)


def one_line(s, n=200):
    return re.sub(r"[\t\r\n']", " ", str(s or ""))[:n].strip()


def title_of(t):
    """'artists - title' of a track object"""
    who = ", ".join(one_line(a, 60) for a in (t.get("artist_names") or []) if a)
    name = one_line(t.get("name"))
    return "%s - %s" % (who, name) if who and name else name or who


class IcyOut:
    """MP3 with a block every METAINT bytes: the track's title, cover and
    EPlay's fields when it changes, else an empty one"""

    def __init__(self):
        self.left, self.sent = METAINT, None

    def wrap(self, data, st):
        out, i = bytearray(), 0
        while i < len(data):
            k = min(self.left, len(data) - i)
            out += data[i:i + k]
            i += k
            self.left -= k
            if not self.left:
                key = st.get("key")
                if key and key != self.sent:
                    t = "StreamTitle='%s';" % st["title"]
                    u = st.get("cover") or ""
                    if u and "'" not in u and all(32 < ord(c) < 127 for c in u) and len(u) < 400:
                        t += "StreamUrl='%s';" % u
                    t += "EPlayTrack='%s';EPlayLen='%d';EPlayPos='%d';" % (st["uri"], st["len"], st["pos"])
                    b = t.encode("utf-8", "replace")[:16 * 255]
                    b += b"\0" * (-len(b) % 16)
                    out += bytes([len(b) // 16]) + b
                    self.sent = key
                else:
                    out += b"\0"
                self.left = METAINT
        return bytes(out)


def start_ffmpeg():
    """ffmpeg from the player's sound (44.1 kHz, stereo, 16 bits) to MP3"""
    args = [Config.ffmpeg, "-hide_banner", "-loglevel", "error", "-nostdin", "-protocol_whitelist", "pipe",
            "-threads", "1", "-f", "s16le", "-ar", "44100", "-ac", "2", "-i", "pipe:0", "-threads", "1",
            "-c:a", "libmp3lame", "-b:a", "%dk" % KBPS, "-f", "mp3", "-flush_packets", "1", "pipe:1"]
    p = subprocess.Popen(args, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL)
    try:
        resource.prlimit(p.pid, resource.RLIMIT_DATA, (Config.memory, Config.memory))
        resource.prlimit(p.pid, resource.RLIMIT_CORE, (0, 0))
        os.setpriority(os.PRIO_PROCESS, p.pid, 10)
    except (OSError, ValueError):
        pass
    return p


class Handler(BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.0"
    server_version = "spotgate/" + VERSION
    timeout = 60

    def log_message(self, fmt, *args):
        if Config.verbose and getattr(self, "command", None):
            aeragate.say("spotgate: %s %s %s" % (self.client_address[0], self.command,
                                                  urlsplit(self.path).path[:20]))

    def reply(self, status, text, ctype="text/plain"):
        body = (text if status == 200 else "spotgate: %s\n" % text).encode("utf-8", "replace")
        try:
            self.send_response(status)
            self.send_header("Content-Type", ctype + ("; charset=utf-8" if status == 200 else ""))
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)
        except OSError:
            pass
        if status != 200:
            aeragate.say("spotgate: %s %d (%s)" % (self.client_address[0], status, text))

    def do_GET(self):
        if not aeragate.client_ok(self.client_address[0], Config.allow):
            self.reply(403, "not from this computer")
            return
        u = urlsplit(self.path)
        q = dict(parse_qsl(u.query, keep_blank_values=True))
        try:
            if u.path == "/version":
                self.reply(200, "spotgate %s, ffmpeg %s\n" % (VERSION, radiogate.ffmpeg_version() or "none"))
            elif u.path == "/login":
                self.reply(200, self.login())
            elif u.path == "/tracks":
                self.reply(200, self.tracks(q.get("uri", "")))
            elif u.path == "/stream":
                self.listen(q)
            else:
                self.reply(404, "no such page")
        except Refused as e:
            if getattr(self, "begun", False):
                aeragate.say("spotgate: %s stopped (%s)" % (self.client_address[0], e.words))
            else:
                self.reply(e.status, e.words)

    def login(self):
        st, js = api("GET", "/auth/code")
        if st == 200 and isinstance(js, dict):
            link = next((v for k, v in js.items() if isinstance(v, str) and ("url" in k or "link" in k)), "")
            code = next((v for k, v in js.items() if isinstance(v, str) and "code" in k and v != link), "")
            if link or code:
                return "code\t%s\t%s\n" % (one_line(link, 300), one_line(code, 40))
        st, js = api("GET", "/status")
        who = one_line(js.get("username"), 60) if isinstance(js, dict) else ""
        return "ready\t%s\n" % who

    def tracks(self, uri):
        if not ITEM.match(uri) or TRACK.match(uri):
            raise Refused(400, "not a Spotify album or playlist")
        lines = []
        for tu, t in item_tracks(uri):
            t = t or {}
            who = ", ".join(one_line(a, 60) for a in (t.get("artist_names") or []) if a)
            lines.append("%d\t%s\t%s\t%s\n" % (int(t.get("duration") or 0), who, one_line(t.get("name")), tu))
        return "".join(lines)

    def where(self, q):
        """(item, track to start at or None, ms into it) from the request"""
        uri, frm = q.get("uri", ""), q.get("from", "")
        if not ITEM.match(uri):
            raise Refused(400, "not a Spotify track, album or playlist")
        try:
            pos = max(0, int(q.get("pos", "0") or 0))
            step = int(q.get("step", "0") or 0)
        except ValueError:
            raise Refused(400, "pos and step are numbers")
        if frm and not TRACK.match(frm):
            raise Refused(400, "from is a Spotify track")
        if not frm or TRACK.match(uri):
            return uri, None, pos
        if step:
            order = [t for t, _ in item_tracks(uri)]
            if frm not in order:
                raise Refused(502, "the track isn't in it any more")
            i = order.index(frm) + (1 if step > 0 else -1)
            if i < 0 or i >= len(order):
                raise Refused(404, "no track %s it" % ("after" if step > 0 else "before"))
            return uri, order[i], 0
        return uri, frm, pos

    def listen(self, q):
        uri, skip, pos = self.where(q)
        meta = self.headers.get("Icy-MetaData", "") == "1"
        if not radiogate.ffmpeg_version():
            raise Refused(502, "Spotify needs ffmpeg on the Gateway")
        # (Next, Previous, a seek: EPlay closes the last stream and asks at
        # once; its relay sees the close at its next write, so up to 3 s)
        if not ONE.acquire(timeout=Config.busy_s):
            raise Refused(503, "Spotify is playing to another Atari")
        t0, sent = time.time(), [0]
        why = "ended"
        try:
            body = {"uri": uri}
            if skip:
                body["skip_to_uri"] = skip
            if pos:
                body["position"] = pos
            st, _ = api("POST", "/player/play", body)
            if st != 200:
                raise Refused(502, "the Spotify player refused it (%d)" % st)
            why = self.relay(meta, sent)
        except (OSError, ValueError):
            why = "the Atari closed it"
        finally:
            try:
                api("POST", "/player/pause", timeout=5)
            except Refused:
                pass
            ONE.release()
        aeragate.say("spotgate: %s listened for %d s, %d KB (%s)" %
                     (self.client_address[0], time.time() - t0, sent[0] // 1024, why))

    def relay(self, meta, sent):
        """the player's sound from its FIFO through ffmpeg to the Atari,
        its state asked every Config.poll s for the blocks"""
        p = start_ffmpeg()
        stop = threading.Event()
        st = {"key": None, "name": "", "ended": False}
        fault = []

        def feed():
            # (the FIFO opened without waiting for the player, and read only
            # while this stream lasts: a reader left blocked in open() or
            # read() after its stream ended took the next stream's sound)
            fd, got = -1, False
            try:
                fd = os.open(Config.fifo, os.O_RDONLY | os.O_NONBLOCK)
                while not stop.is_set():
                    r, _, _ = select.select([fd], [], [], 0.25)
                    if not r or stop.is_set():
                        continue
                    try:
                        d = os.read(fd, 16384)
                    except BlockingIOError:
                        continue
                    if not d:
                        if got:
                            break           # (the player closed it: the item's end)
                        time.sleep(0.05)    # (no writer yet)
                        continue
                    got = True
                    p.stdin.write(d)
                    p.stdin.flush()
            except (OSError, ValueError):
                if not stop.is_set():
                    fault.append("the player's sound failed")
            finally:
                if fd >= 0:
                    os.close(fd)
                try:
                    p.stdin.close()
                except OSError:
                    pass

        def watch():
            began, quiet = time.time(), None
            while not stop.is_set():
                try:
                    code, js = api("GET", "/status", timeout=5)
                except Refused:
                    code, js = 0, None
                if code == 200 and isinstance(js, dict):
                    st["name"] = one_line(js.get("context_name"), 120)
                    t = js.get("track") if isinstance(js.get("track"), dict) else None
                    if t and isinstance(t.get("uri"), str):
                        if t["uri"] != (st.get("uri") if st["key"] else None):
                            st.update(uri=one_line(t["uri"], 60), title=title_of(t),
                                      cover=t.get("album_cover_url") or "", len=int(t.get("duration") or 0),
                                      pos=int(t.get("position") or 0))
                            st["key"] = (st["uri"], time.time())
                    stopped = bool(js.get("stopped"))
                else:
                    stopped = code == 204
                # (the item's end: the player stopped by itself, 3 s on)
                if stopped and time.time() - began > Config.start_s:
                    quiet = quiet or time.time()
                    if time.time() - quiet > 3:
                        st["ended"] = True
                        stop.set()
                        try:
                            p.kill()
                        except OSError:
                            pass
                        return
                else:
                    quiet = None
                stop.wait(Config.poll)

        last = [time.time(), False]     # (the last sound's time; any yet)

        def dog():
            # (no sound within start_s of the start, or for idle_s once
            # begun: ffmpeg killed, so the read ends and ONE is free again,
            # whatever the player's status says: the Gateway's review)
            while not stop.is_set():
                limit = Config.idle_s if last[1] else Config.first_s
                if time.time() - last[0] > limit:
                    fault.append("the Spotify player sent no sound for %d s" % limit)
                    stop.set()
                    try:
                        p.kill()
                    except OSError:
                        pass
                    return
                stop.wait(0.25)

        threading.Thread(target=feed, daemon=True).start()
        threading.Thread(target=dog, daemon=True).start()
        threading.Thread(target=watch, daemon=True).start()
        icy = IcyOut() if meta else None
        try:
            first = p.stdout.read1(16384)
            last[:] = [time.time(), True]
            if not first:
                raise Refused(502, fault[0] if fault else "the Spotify player sent no sound")
            # (the track's details before its sound: its block then comes
            # at the first 8 KB, where EPlay shows it as it is heard)
            t0 = time.time()
            while meta and not st["key"] and time.time() - t0 < 3 and not stop.is_set():
                time.sleep(0.05)
            self.begun = True
            self.send_response(200)
            self.send_header("Content-Type", "audio/mpeg")
            self.send_header("icy-name", st["name"] or "Spotify")
            self.send_header("icy-br", str(KBPS))
            if meta:
                self.send_header("icy-metaint", str(METAINT))
            self.send_header("Cache-Control", "no-cache")
            self.end_headers()
            data = first
            while data:
                out = icy.wrap(data, st) if icy else data
                self.wfile.write(out)
                self.wfile.flush()
                sent[0] += len(out)
                data = p.stdout.read1(16384)
                last[0] = time.time()
            return "the item's end" if st["ended"] else (fault[0] if fault else "ended")
        finally:
            stop.set()
            p.kill()
            p.wait()
            p.stdout.close()


def parser():
    p = argparse.ArgumentParser(description="Spotify for EPlay through the Gateway")
    p.add_argument("--bind", default="0.0.0.0")
    p.add_argument("--port", type=int, default=8581)
    p.add_argument("--allow", action="append", default=[], help="the Atari's address (again for more)")
    p.add_argument("--api", default=Config.api, help="go-librespot's API (on this computer)")
    p.add_argument("--fifo", default=Config.fifo, help="go-librespot's audio_output_pipe")
    p.add_argument("--ffmpeg", default="ffmpeg")
    p.add_argument("--memory-mb", type=int, default=256, help="ffmpeg's memory")
    p.add_argument("-v", "--verbose", action="store_true")
    return p


def main(argv=None):
    a = parser().parse_args(argv)
    if urlsplit(a.api).hostname not in ("127.0.0.1", "localhost", "::1"):
        raise SystemExit("spotgate: --api is the player on this computer")
    Config.allow, Config.api, Config.fifo = a.allow, a.api.rstrip("/"), a.fifo
    Config.ffmpeg = radiogate.Config.ffmpeg = shutil.which(a.ffmpeg) or a.ffmpeg
    Config.memory = a.memory_mb << 20
    Config.verbose = a.verbose
    srv = ThreadingHTTPServer((a.bind, a.port), Handler)
    srv.daemon_threads = True
    aeragate.say("spotgate %s listening on %s:%d" % (VERSION, a.bind, a.port))
    try:
        srv.serve_forever()
    except KeyboardInterrupt:
        pass


if __name__ == "__main__":
    main()
