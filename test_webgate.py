# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
"""Tests for webgate: the proxy against a local server, as EWeb uses it."""

import gzip
import http.client
import os
import sys
import threading
import unittest
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

sys.path.insert(0, os.path.dirname(__file__))
import webgate  # noqa: E402

PAGE = b"<!DOCTYPE html><title>Page</title><p>\xd7\xa9\xd7\x9c\xd7\x95\xd7\x9d"
BIG = bytes(range(256)) * 1024          # 256 KB, more than one piece


class Upstream(BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"
    seen = []

    def log_message(self, *a):
        pass

    def answer(self):
        n = int(self.headers.get("Content-Length") or 0)
        body = self.rfile.read(n) if n else b""
        Upstream.seen.append((self.command, self.path, dict(self.headers), body))
        if self.path == "/page":
            data = gzip.compress(PAGE) if "gzip" in self.headers.get("Accept-Encoding", "") else PAGE
            self.send_response(200)
            self.send_header("Content-Type", "text/html; charset=utf-8")
            if data is not PAGE:
                self.send_header("Content-Encoding", "gzip")
            self.send_header("Set-Cookie", "a=1; Path=/")
            self.send_header("Set-Cookie", "b=2; Path=/")
            self.send_header("Content-Length", str(len(data)))
            self.end_headers()
            if self.command != "HEAD":
                self.wfile.write(data)
        elif self.path == "/big":
            self.send_response(200)
            self.send_header("Content-Type", "application/octet-stream")
            self.send_header("Transfer-Encoding", "chunked")
            self.end_headers()
            for i in range(0, len(BIG), 10000):
                part = BIG[i:i + 10000]
                self.wfile.write(b"%x\r\n%s\r\n" % (len(part), part))
            self.wfile.write(b"0\r\n\r\n")
        elif self.path == "/moved":
            self.send_response(302)
            self.send_header("Location", "/page")
            self.send_header("Content-Length", "0")
            self.end_headers()
        elif self.path == "/same":
            self.send_response(304)
            self.send_header("ETag", '"v1"')
            self.end_headers()
        elif self.path.startswith("/echo"):
            data = b"%s %s %s" % (self.command.encode(), self.path.encode(), body)
            self.send_response(200)
            self.send_header("Content-Type", "text/plain")
            self.send_header("Content-Length", str(len(data)))
            self.end_headers()
            self.wfile.write(data)
        else:
            data = b"none"
            self.send_response(404)
            self.send_header("Content-Length", str(len(data)))
            self.end_headers()
            self.wfile.write(data)

    do_GET = do_POST = do_HEAD = answer


class ProxyTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.up = ThreadingHTTPServer(("127.0.0.1", 0), Upstream)
        threading.Thread(target=cls.up.serve_forever, daemon=True).start()
        webgate.Config.allow_local = True
        cls.gw = ThreadingHTTPServer(("127.0.0.1", 0), webgate.Handler)
        cls.gw.daemon_threads = True
        threading.Thread(target=cls.gw.serve_forever, daemon=True).start()
        cls.base = "http://127.0.0.1:%d" % cls.up.server_address[1]

    @classmethod
    def tearDownClass(cls):
        cls.up.shutdown()
        cls.gw.shutdown()
        cls.up.server_close()
        cls.gw.server_close()
        webgate.Config.allow_local = False

    def ask(self, method, path, headers=None, body=None, conn=None):
        c = conn or http.client.HTTPConnection("127.0.0.1", self.gw.server_address[1], timeout=10)
        if conn is None:
            self.addCleanup(c.close)
        c.request(method, self.base + path, body=body, headers=headers or {})
        r = c.getresponse()
        return r, r.read(), c

    def test_page_gzip_passed_through(self):
        r, data, _ = self.ask("GET", "/page", {"Accept-Encoding": "gzip, deflate, br"})
        self.assertEqual(r.status, 200)
        self.assertEqual(r.getheader("Content-Encoding"), "gzip")
        self.assertEqual(gzip.decompress(data), PAGE)
        # only what the Atari unpacks was asked for
        self.assertEqual(Upstream.seen[-1][2]["Accept-Encoding"], "gzip, deflate")
        self.assertIsNone(r.getheader("X-Gateway-TLS"))       # plain HTTP upstream

    def test_identity_and_cookies(self):
        r, data, _ = self.ask("GET", "/page")
        self.assertEqual(data, PAGE)
        self.assertEqual(Upstream.seen[-1][2]["Accept-Encoding"], "identity")
        self.assertEqual([v for k, v in r.getheaders() if k.lower() == "set-cookie"], ["a=1; Path=/", "b=2; Path=/"])

    def test_chunked_big(self):
        r, data, _ = self.ask("GET", "/big")
        self.assertEqual(r.getheader("Transfer-Encoding"), "chunked")
        self.assertEqual(data, BIG)

    def test_redirect_and_304_and_404_as_they_came(self):
        r, data, c = self.ask("GET", "/moved")
        self.assertEqual((r.status, r.getheader("Location")), (302, "/page"))
        r, data, c = self.ask("GET", "/same", {"If-None-Match": '"v1"'}, conn=c)
        self.assertEqual((r.status, data), (304, b""))
        self.assertEqual(Upstream.seen[-1][2]["If-None-Match"], '"v1"')
        r, data, c = self.ask("GET", "/nothing", conn=c)
        self.assertEqual((r.status, data), (404, b"none"))

    def test_post_and_head_kept_connection(self):
        r, data, c = self.ask("POST", "/echo?x=1", {"Content-Type": "application/x-www-form-urlencoded"},
                              b"q=atari")
        self.assertEqual(data, b"POST /echo?x=1 q=atari")
        r, data, c = self.ask("HEAD", "/page", conn=c)
        self.assertEqual((r.status, data), (200, b""))
        r, data, c = self.ask("GET", "/echo", conn=c)
        self.assertEqual(data, b"GET /echo ")

    def test_status_page_and_bad_requests(self):
        c = http.client.HTTPConnection("127.0.0.1", self.gw.server_address[1], timeout=10)
        self.addCleanup(c.close)
        c.request("GET", "/")
        r = c.getresponse()
        self.assertEqual(r.status, 200)
        self.assertIn(b"webgate", r.read())
        c.request("GET", "ftp://example.com/x")
        r = c.getresponse()
        self.assertEqual(r.status, 400)
        self.assertTrue(r.read().startswith(b"webgate: "))

    def test_unknown_host(self):
        c = http.client.HTTPConnection("127.0.0.1", self.gw.server_address[1], timeout=20)
        self.addCleanup(c.close)
        c.request("GET", "http://no-such-host.invalid/")
        r = c.getresponse()
        self.assertEqual(r.status, 502)
        self.assertIn(b"webgate: can't find no-such-host.invalid", r.read())

    def test_home_network_refused(self):
        webgate.Config.allow_local = False
        try:
            r, data, _ = self.ask("GET", "/page")
        finally:
            webgate.Config.allow_local = True
        self.assertEqual(r.status, 403)
        self.assertIn(b"webgate: won't connect to the local network", data)


class TlsTest(unittest.TestCase):
    def test_tls_of(self):
        self.assertEqual(webgate.tls_of(http.client.HTTPConnection("x")), "")
        fake = type("C", (), {})()
        fake.sock = None
        self.assertEqual(webgate.tls_of(fake), "")


if __name__ == "__main__":
    unittest.main()
