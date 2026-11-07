#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
"""translategate's engine on a Pi: quality, speed and memory, from the fixed
sets in tests/translate/ (no private text).

    python3 tools/translate-check.py --port 8480        (on the Gateway itself)

It says first what it runs on (the machine, the system and whether it is
64-bit, the memory and swap), and how much memory translategate takes.

The sets: he-en.tsv, letters and sentences; he-en-short.tsv, a few words,
closings, names and subjects. For each line: the time the answer took, a
chrF score against the reference (character n-grams, 0-100; a rough
measure, read the answers too), and whether the URLs and numbers came
back as they were. Then a piece of 2 KB each way, the size the apps
send: the seconds it takes. translategate's memory is read from its
process (systemd's MainPID, or --pid), resident and at its peak.
Standard Python 3 only.
"""
import argparse
import os
import platform
import re
import subprocess
import time
import urllib.error
import urllib.request
from collections import Counter

HERE = os.path.dirname(os.path.abspath(__file__))
SETS = [os.path.join(HERE, "..", "tests", "translate", f) for f in ("he-en.tsv", "he-en-short.tsv")]


def chrf(hyp, ref, n=6, beta=2.0):
    """chrF (Popovic 2015): character n-gram F-score, spaces left out"""
    hyp, ref = re.sub(r"\s+", "", hyp), re.sub(r"\s+", "", ref)
    ps, rs = [], []
    for k in range(1, n + 1):
        h = Counter(hyp[i:i + k] for i in range(len(hyp) - k + 1))
        r = Counter(ref[i:i + k] for i in range(len(ref) - k + 1))
        if not h or not r:
            continue
        m = sum((h & r).values())
        ps.append(m / sum(h.values()))
        rs.append(m / sum(r.values()))
    if not ps:
        return 0.0
    p, r = sum(ps) / len(ps), sum(rs) / len(rs)
    return 0.0 if p + r == 0 else 100 * (1 + beta ** 2) * p * r / (beta ** 2 * p + r)


def memory(pid):
    """the process's memory: (resident now, at its peak) in MB"""
    got = {}
    try:
        with open("/proc/%d/status" % pid) as f:
            for line in f:
                k, _, v = line.partition(":")
                if k in ("VmRSS", "VmHWM"):
                    got[k] = int(v.split()[0]) // 1024
    except (OSError, ValueError):
        pass
    return "resident %s MB, peak %s MB" % (got.get("VmRSS", "?"), got.get("VmHWM", "?"))


def meminfo():
    got = {}
    try:
        with open("/proc/meminfo") as f:
            for line in f:
                k, _, v = line.partition(":")
                got[k] = int(v.split()[0]) // 1024
    except (OSError, ValueError, IndexError):
        pass
    return got


def machine():
    """this computer, as Erez would name it: the model, the system and its bits, the memory"""
    model = ""
    for p in ("/proc/device-tree/model", "/sys/firmware/devicetree/base/model",
              "/sys/devices/virtual/dmi/id/product_name"):
        try:
            with open(p, "rb") as f:
                model = f.read().decode("utf-8", "replace").strip("\0 \n")
            if model:
                break
        except OSError:
            pass
    system = platform.system()
    try:
        with open("/etc/os-release") as f:
            for line in f:
                if line.startswith("PRETTY_NAME="):
                    system = line.split("=", 1)[1].strip().strip('"')
    except OSError:
        pass
    arch = platform.machine()
    bits = "64-bit" if arch in ("aarch64", "arm64", "x86_64") else "32-bit" if arch.startswith(("armv", "i")) else ""
    m = meminfo()
    zram = os.path.exists("/sys/block/zram0")
    lines = ["machine:  %s" % (model or "?"),
             "system:   %s, %s %s" % (system, arch, bits),
             "memory:   %s MB, %s MB free now; swap %s MB%s" % (
                 m.get("MemTotal", "?"), m.get("MemAvailable", "?"), m.get("SwapTotal", "?"),
                 " (zram)" if zram else "")]
    return "\n".join(lines)


def service_pid():
    try:
        out = subprocess.run(["systemctl", "show", "-p", "MainPID", "--value", "translategate"],
                             capture_output=True, text=True, timeout=10).stdout.strip()
        return int(out) or None
    except (OSError, ValueError, subprocess.SubprocessError):
        return None


def pieces(base, path):
    """seconds for a piece of 2 KB, the size the apps send, each way: the
    first one, and the mean of three"""
    texts = {}
    for line in open(path, encoding="utf-8"):
        if line.startswith("#") or not line.strip():
            continue
        src, dst, text, _ = line.rstrip("\n").split("\t")
        texts.setdefault((src, dst), []).append(text.replace("\\n", "\n"))
    for (src, dst), ts in sorted(texts.items()):
        body, paras = "", []
        while len(body.encode()) < 2048:
            paras.append(" ".join(ts))
            body = "\n\n".join(paras)
        data = body.encode()[:2048].decode("utf-8", "ignore").encode()
        took = []
        for _ in range(3):
            req = urllib.request.Request("%s/translate?to=%s&from=%s" % (base, dst, src), data=data,
                                         headers={"Content-Type": "text/plain; charset=utf-8"})
            t0 = time.monotonic()
            try:
                with urllib.request.urlopen(req, timeout=70) as r:
                    r.read()
            except urllib.error.HTTPError as e:
                print("%s>%s a 2 KB piece: %d %s" % (src, dst, e.code, e.read().decode("utf-8", "replace").strip()))
                break
            took.append(time.monotonic() - t0)
        if took:
            print("%s>%s a 2 KB piece (%d bytes): %.1f s the first, %.1f s on average (the apps wait 60 s at most)"
                  % (src, dst, len(data), took[0], sum(took) / len(took)))


def kept(text):
    return re.findall(r"https?://\S+|\d+(?:[.,:]\d+)*", text)


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--host", default="127.0.0.1")
    ap.add_argument("--port", type=int, default=8480)
    ap.add_argument("--pid", type=int)
    ap.add_argument("--set", action="append", help="a set of lines (default: the long and the short one)")
    a = ap.parse_args()
    base = "http://%s:%d" % (a.host, a.port)
    here = a.host in ("127.0.0.1", "localhost", "::1")
    if here:
        print(machine())
    pid = a.pid or (service_pid() if here else None)
    with urllib.request.urlopen(base + "/status", timeout=10) as r:
        print("service:  " + r.read().decode().replace("\n", ", ").rstrip(", "))
    if pid:
        print("translategate: %s (the models loaded)" % memory(pid))
    for path in a.set or SETS:
        check(base, path)
    print("== pieces of 2 KB")
    pieces(base, (a.set or SETS)[0])
    if pid:
        print("translategate after: %s" % memory(pid))
    if here:
        print("memory free after: %s MB" % meminfo().get("MemAvailable", "?"))


def check(base, path):
    print("== %s" % os.path.basename(path))
    scores, times = [], []
    for line in open(path, encoding="utf-8"):
        if line.startswith("#") or not line.strip():
            continue
        src, dst, text, ref = line.rstrip("\n").split("\t")
        text, ref = text.replace("\\n", "\n"), ref.replace("\\n", "\n")
        data = text.encode()
        req = urllib.request.Request("%s/translate?to=%s&from=%s" % (base, dst, src), data=data,
                                     headers={"Content-Type": "text/plain; charset=utf-8"})
        t0 = time.monotonic()
        try:
            with urllib.request.urlopen(req, timeout=70) as r:
                out = r.read().decode("utf-8")
        except urllib.error.HTTPError as e:
            out = "(%d %s)" % (e.code, e.read().decode("utf-8", "replace").strip())
        took = time.monotonic() - t0
        s = chrf(out, ref)
        ok = all(k in out for k in kept(text))
        scores.append(s)
        times.append(took)
        print("%s>%s %5.2f s  chrF %5.1f  %s\n    %s\n    %s" %
              (src, dst, took, s, "kept" if ok else "LOST A URL OR NUMBER",
               out.replace("\n", " / "), ref.replace("\n", " / ")))
    print("%d lines: chrF %.1f on average, %.2f s on average, %.2f s at most" %
          (len(scores), sum(scores) / len(scores), sum(times) / len(times), max(times)))


if __name__ == "__main__":
    main()
