#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
"""translategate's engine on a Pi: quality, speed and memory, from the fixed
sets in tests/translate/ (no private text).

    python3 tools/translate-check.py [--host PI_IP] [--port PORT] [--pid PID]

The sets: he-en.tsv, letters and sentences; he-en-short.tsv, a few words,
closings, names and subjects. For each line: the time the answer took, a
chrF score against the reference (character n-grams, 0-100; a rough
measure, read the answers too), and whether the URLs and numbers came
back as they were. With --pid (the service's: systemctl show -p MainPID
--value translategate), its memory before and after, resident and at
its peak. Standard Python 3 only.
"""
import argparse
import os
import re
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


def kept(text):
    return re.findall(r"https?://\S+|\d+(?:[.,:]\d+)*", text)


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--host", default="127.0.0.1")
    ap.add_argument("--port", type=int, required=True)
    ap.add_argument("--pid", type=int)
    ap.add_argument("--set", action="append", help="a set of lines (default: the long and the short one)")
    a = ap.parse_args()
    base = "http://%s:%d" % (a.host, a.port)
    with urllib.request.urlopen(base + "/status", timeout=10) as r:
        print(r.read().decode().replace("\n", ", ").rstrip(", "))
    if a.pid:
        print("memory before: %s" % memory(a.pid))
    for path in a.set or SETS:
        check(base, path)
    if a.pid:
        print("memory after: %s" % memory(a.pid))


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
