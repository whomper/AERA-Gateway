#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
"""webgate - the Raspberry Pi gateway for EWeb on the Atari.

EWeb on a Falcon makes its own secure connections (TLS 1.2, the curves on
the DSP); the ST edition, EWEBST.PRG, can't, and some sites take TLS 1.3
only. So EWeb, when its gateway is set, sends every request to the Pi in
plain HTTP on your home network, in proxy form:

    GET https://en.wikipedia.org/wiki/Atari_ST HTTP/1.1

and webgate makes the request, checks the server's certificate (TLS 1.2
or 1.3) and hands the answer back as it comes:

  - the server's own compression (gzip, deflate) is passed through as it
    is, and only those are asked for: less to carry over a serial line,
    and EWeb unpacks them;
  - a long answer is passed on a piece at a time (chunked when its length
    isn't known), so a download needn't fit in the Pi's memory either;
  - redirects, cookies and error pages go back as they came: EWeb follows
    them and keeps the cookies itself;
  - X-Gateway-TLS says how the Pi reached the server ("TLS 1.3,
    TLS_AES_128_GCM_SHA256"), for EWeb's info line;
  - one connection per server is kept open and reused.

A request that can't be made is answered 502 (403 for the home network,
504 for a server that is too slow) with a line starting "webgate:", which
EWeb shows on its "can't open" page.

Only standard Python 3 is needed, and aeragate.py next to it.
`aera-gateway install web` sets it up as a service.

    webgate.py [--port 8280] [--allow 192.168.1.50] [--allow-local] [--cafile F] [-v]
"""

import argparse
import http.client
import os
import socket
import ssl
import threading
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import urlsplit

import aeragate

VERSION = "1.0"

HOP_BY_HOP = {
    "connection", "keep-alive", "proxy-connection", "proxy-authorization",
    "proxy-authenticate", "te", "trailer", "transfer-encoding", "upgrade",
    "host", "content-length", "accept-encoding",
}

PIECE = 65536


class Config:
    allow = []              # client addresses allowed; empty = any on the private networks
    allow_local = False     # may requests go to servers on the local network?
    verbose = False
    timeout = 60
    cafile = None           # the root certificates to check servers against (None: the Pi's)
    insecure = False        # tests only: don't check certificates


def _dial(address, timeout=None, source_address=None):
    """a connection to the address that was checked: without --allow-local
    never one on the home network, even if the name's answer changes"""
    return aeragate.connect(address[0], address[1], timeout, Config.allow_local)


def _proxy(scheme):
    """(host, port) of an upstream proxy from HTTPS_PROXY, for a Pi on a
    network that only goes out through one; None normally."""
    v = os.environ.get(scheme.upper() + "_PROXY") or os.environ.get(scheme + "_proxy")
    if not v:
        return None
    u = urlsplit(v if "://" in v else "http://" + v)
    return (u.hostname, u.port or 8080) if u.hostname else None


class Upstream(threading.local):
    """One kept-open connection per server, per thread (an Atari's
    connection is served by one thread)."""

    def __init__(self):
        self.conns = {}

    def get(self, scheme, host, port):
        key = (scheme, host, port)
        c = self.conns.get(key)
        if c is None:
            if scheme == "https":
                ctx = aeragate.tls_context(Config.cafile)
                if Config.insecure:
                    ctx.check_hostname = False
                    ctx.verify_mode = ssl.CERT_NONE
                proxy = _proxy("https")
                if proxy:
                    c = http.client.HTTPSConnection(proxy[0], proxy[1], timeout=Config.timeout, context=ctx)
                    c.set_tunnel(host, port)
                else:
                    c = http.client.HTTPSConnection(host, port, timeout=Config.timeout, context=ctx)
                    c._create_connection = _dial
            else:
                c = http.client.HTTPConnection(host, port, timeout=Config.timeout)
                c._create_connection = _dial
            self.conns[key] = c
        return c

    def drop(self, scheme, host, port):
        c = self.conns.pop((scheme, host, port), None)
        if c:
            c.close()


UP = Upstream()


def tls_of(conn):
    """how conn reached its server: "TLS 1.3, TLS_AES_128_GCM_SHA256", or
    "" for plain HTTP"""
    s = getattr(conn, "sock", None)
    if not isinstance(s, ssl.SSLSocket):
        return ""
    v = (s.version() or "TLS").replace("TLSv", "TLS ")
    c = s.cipher()
    return "%s, %s" % (v, c[0]) if c else v


class Handler(BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"
    server_version = "webgate/" + VERSION

    where = "-"             # the server a request goes to, for the log

    def log_message(self, fmt, *args):
        if Config.verbose:
            aeragate.say("%s %s" % (self.address_string(), fmt % args))

    def log_request(self, code="-", size="-"):
        # the request line has the whole address, with what a form sent
        # in it: note() logs the server only (7.3)
        pass

    def note(self, status, size, more=""):
        if Config.verbose:
            aeragate.say("%s %s %s -> %d, %d bytes%s" % (
                self.address_string(), self.command, self.where, status, size, more))

    def handle(self):
        try:
            super().handle()
        except (ConnectionResetError, BrokenPipeError, socket.timeout):
            pass        # the Atari went away (reset, switched off, Esc): nothing to say

    def do_GET(self):
        self.proxy()

    do_HEAD = do_POST = do_GET

    def reply(self, status, text, ctype="text/plain; charset=utf-8"):
        body = text.encode("utf-8")
        self.note(status, len(body), ": " + text.strip() if status >= 400 else "")
        self.send_response(status)
        self.send_header("Content-Type", ctype)
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        if self.command != "HEAD":
            self.wfile.write(body)

    def proxy(self):
        self.where = "-"
        if not aeragate.client_ok(self.client_address[0], Config.allow):
            self.reply(403, "webgate: this address may not use the gateway\n")
            return
        length = int(self.headers.get("Content-Length") or 0)
        body = self.rfile.read(length) if length else None
        if self.path.startswith("/"):
            self.reply(200, "webgate %s is running. Put this address in EWeb's gateway setting.\n" % VERSION)
            return
        u = urlsplit(self.path)
        if u.scheme not in ("http", "https") or not u.hostname:
            self.reply(400, "webgate: expected a request like GET https://host/path\n")
            return
        port = u.port or (443 if u.scheme == "https" else 80)
        self.where = "%s://%s:%d" % (u.scheme, u.hostname, port)
        path = (u.path or "/") + ("?" + u.query if u.query else "")
        headers = {k: v for k, v in self.headers.items() if k.lower() not in HOP_BY_HOP}
        headers["Host"] = u.netloc.rsplit("@", 1)[-1]
        # what the Atari unpacks itself, and nothing it can't
        asked = (self.headers.get("Accept-Encoding") or "").lower()
        headers["Accept-Encoding"] = ", ".join(e for e in ("gzip", "deflate") if e in asked) or "identity"
        for attempt in (1, 2):
            conn = UP.get(u.scheme, u.hostname, port)
            try:
                conn.request(self.command, path, body=body, headers=headers)
                resp = conn.getresponse()
                break
            except ssl.SSLCertVerificationError as e:
                UP.drop(u.scheme, u.hostname, port)
                self.reply(502, "webgate: %s has an untrusted certificate (%s)\n" % (u.hostname, e.verify_message))
                return
            except aeragate.HomeNetwork:
                UP.drop(u.scheme, u.hostname, port)
                self.reply(403, "webgate: won't connect to the local network (%s)\n" % u.hostname)
                return
            except socket.gaierror:
                UP.drop(u.scheme, u.hostname, port)
                self.reply(502, "webgate: can't find %s\n" % u.hostname)
                return
            except (http.client.RemoteDisconnected, BrokenPipeError, ConnectionResetError,
                    http.client.CannotSendRequest, http.client.ResponseNotReady):
                UP.drop(u.scheme, u.hostname, port)    # a kept connection went stale
                if attempt == 2:
                    self.reply(502, "webgate: %s closed the connection\n" % u.hostname)
                    return
            except (OSError, http.client.HTTPException) as e:
                UP.drop(u.scheme, u.hostname, port)
                self.reply(504 if isinstance(e, socket.timeout) else 502,
                           "webgate: can't reach %s (%s)\n" % (u.hostname, e))
                return
        self.pass_on(resp, conn, u, port)

    def pass_on(self, resp, conn, u, port):
        """the answer to the Atari as it comes, a piece at a time"""
        clen = resp.getheader("Content-Length")
        known = clen is not None and clen.strip().isdigit() and resp.chunked is False
        empty = self.command == "HEAD" or resp.status in (204, 304) or 100 <= resp.status < 200
        self.send_response(resp.status, resp.reason)
        for k, v in resp.getheaders():
            if k.lower() not in HOP_BY_HOP:
                self.send_header(k, v)
        tls = tls_of(conn)
        if tls:
            self.send_header("X-Gateway-TLS", tls)
        if empty:
            self.send_header("Content-Length", "0")
        elif known:
            self.send_header("Content-Length", clen.strip())
        else:
            self.send_header("Transfer-Encoding", "chunked")
        self.end_headers()
        sent = 0
        try:
            while not empty:
                piece = resp.read(PIECE)
                if not piece:
                    break
                sent += len(piece)
                self.wfile.write(piece if known else b"%x\r\n%s\r\n" % (len(piece), piece))
            if not empty and not known:
                self.wfile.write(b"0\r\n\r\n")
            resp.read()
        except (OSError, http.client.HTTPException):
            # the server or the Atari went away halfway: neither connection
            # can carry another answer
            UP.drop(u.scheme, u.hostname, port)
            self.close_connection = True
        if resp.will_close:
            UP.drop(u.scheme, u.hostname, port)
        self.note(resp.status, sent)


def parser():
    """the command line (aera-gateway reads a service's settings with it)"""
    ap = argparse.ArgumentParser(description="HTTP and HTTPS gateway for EWeb on the Atari")
    ap.add_argument("--port", type=int, default=8280)
    ap.add_argument("--bind", default="0.0.0.0")
    ap.add_argument("--allow", action="append", default=[],
                    help="only this Atari's address may use the gateway (may repeat)")
    ap.add_argument("--allow-local", action="store_true",
                    help="also forward to servers on the local network")
    ap.add_argument("--cafile", help="check servers against these root certificates, not the Pi's")
    ap.add_argument("--insecure", action="store_true", help=argparse.SUPPRESS)
    ap.add_argument("-v", "--verbose", action="store_true")
    return ap


def main(argv=None):
    a = parser().parse_args(argv)
    Config.allow = a.allow
    Config.allow_local = a.allow_local
    Config.verbose = a.verbose
    Config.cafile = a.cafile
    Config.insecure = a.insecure
    srv = ThreadingHTTPServer((a.bind, a.port), Handler)
    srv.daemon_threads = True
    aeragate.say("webgate %s listening on %s:%d" % (VERSION, a.bind, a.port))
    try:
        srv.serve_forever()
    except KeyboardInterrupt:
        pass


if __name__ == "__main__":
    main()
