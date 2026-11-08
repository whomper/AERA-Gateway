#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
"""translateclient - translategate for the Gateway's own gateways.

webgate and pdfgate translate on the Gateway (Erez, 8 Nov 2026) through
translategate's local socket: the same machine, one engine with its
models loaded once, translategate the only process that has them. The
words are POST /translate's (AERA-Atari standards/TRANSLATION-SERVICE.md):
the text shaped the same way, the same refusals.

    translate(text, to, src=None) -> (translation, from, same)
    translate_parts(parts, to, src=None, seconds=30) -> ([translations], from, done)

Refused(status, line) is raised with translategate's words ("that pair
of languages isn't installed", 503 while it loads, 504 over its time);
Unreachable when there is no translategate. Nothing is kept or logged
here. Standard Python 3 only.
"""

import socket
import time

SOCKET = "/run/translategate/local.sock"


class Refused(Exception):
    """translategate's refusal: status (400, 413, 422, 503, 504, 500) and
    its one line, "translategate: why", as the Atari would show it"""

    def __init__(self, status, line, used=None):
        super().__init__("%d %s" % (status, line))
        self.status, self.line, self.used = status, line, used


class Unreachable(Exception):
    """no translategate on this machine (not installed, or starting)"""


def ask(method, path, body=b"", sock=SOCKET, timeout=620):
    """(status, headers in lower case, body) for one request"""
    s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    s.settimeout(timeout)
    try:
        s.connect(sock)
    except OSError as e:
        s.close()
        raise Unreachable("translategate isn't there (%s)" % type(e).__name__)
    try:
        head = "%s %s HTTP/1.0\r\nHost: translategate\r\n" % (method, path)
        if method == "POST":
            head += "Content-Type: text/plain; charset=utf-8\r\nContent-Length: %d\r\n" % len(body)
        s.sendall(head.encode() + b"\r\n" + body)
        got = b""
        while True:
            b = s.recv(65536)
            if not b:
                break
            got += b
    except OSError as e:
        raise Unreachable("translategate stopped answering (%s)" % type(e).__name__)
    finally:
        s.close()
    top, _, data = got.partition(b"\r\n\r\n")
    lines = top.decode("latin-1").split("\r\n")
    try:
        status = int(lines[0].split()[1])
    except (IndexError, ValueError):
        raise Unreachable("that isn't translategate")
    hd = {}
    for line in lines[1:]:
        k, _, v = line.partition(":")
        hd[k.strip().lower()] = v.strip()
    return status, hd, data


def translate(text, to, src=None, sock=SOCKET):
    """text (str) into the language to: (translation, the language it was
    in, same). same: it was in that language already, and translation is
    text as it came"""
    path = "/translate?to=%s" % to + ("&from=%s" % src if src else "")
    status, hd, data = ask("POST", path, text.encode("utf-8"), sock)
    used = hd.get("x-translate-from")
    if status != 200:
        raise Refused(status, data.decode("utf-8", "replace").strip(), used)
    if hd.get("x-translate-same") == "1":
        return text, used, True
    return data.decode("utf-8"), used, False


def translate_parts(parts, to, src=None, seconds=30, sock=SOCKET, piece=2048):
    """a page's or a text's parts (paragraphs, in order) translated as
    far as seconds allow: (translations of the first done parts, the
    language, done). Parts go a few at a time, about piece bytes, so the
    first ones come back whatever the rest would take; the caller leaves
    parts[done:] as they were and says how much was translated (Erez,
    8 Nov 2026: time is the limit)"""
    out, used, end = [], src, time.monotonic() + seconds
    i = 0
    while i < len(parts) and time.monotonic() < end:
        j, size = i, 0
        while j < len(parts) and (j == i or size + len(parts[j].encode("utf-8")) <= piece):
            size += len(parts[j].encode("utf-8")) + 2
            j += 1
        text, used_now, same = translate("\n\n".join(parts[i:j]), to, used, sock)
        got = parts[i:j] if same else text.split("\n\n")
        if len(got) != j - i:           # (the shaping keeps paragraphs; if not, one at a time)
            got = [translate(p, to, used, sock)[0] for p in parts[i:j]]
        used = used or used_now
        out += got
        i = j
    return out, used, len(out)
