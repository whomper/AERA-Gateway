#!/bin/bash
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
# install web --browser on Ubuntu, whose Chromium is a snap, for real: the
# snap started by webgate's own systemd, a page through it as a snapshot
# (its script run), live view's picture at EWeb's size, and all of it
# gone again with remove. CI's Ubuntu runner, or a spare Ubuntu computer.
#
#   sudo tests/snap.sh            (needs the network: it fetches the snap)
set -euo pipefail
GW=$(cd "$(dirname "$0")/../gateway" && pwd)
W=$(mktemp -d)
fail() { echo "FAIL: $*" >&2; exit 1; }
ok() { echo "ok: $*"; }

[ "$(id -u)" = 0 ] || fail "run it with sudo"
snap list chromium >/dev/null 2>&1 || snap install chromium
# a CI runner's own Chromium isn't Ubuntu's: the snap is what is tested
for f in /usr/bin/chromium /usr/bin/chromium-browser; do
  case "$(readlink -f "$f" 2>/dev/null)" in /snap/*|/usr/bin/snap|"") ;; *) rm -f "$f" ;; esac
done

echo "== install web --browser with Ubuntu's Chromium snap"
"$GW/aera-gateway" install web --browser --allow-local
U=$(id -u webgate)
[ "$(getent passwd webgate | cut -d: -f6)" = /var/lib/webgate ] || fail "webgate's home isn't /var/lib/webgate"
loginctl show-user webgate -p Linger | grep -qx Linger=yes || fail "webgate doesn't linger"
[ ! -e /etc/systemd/system/webgate-browser.service ] || fail "a system unit for the snap"
for i in $(seq 1 30); do
  systemctl --user -M webgate@ is-active --quiet webgate-browser.service && break; sleep 1
done
systemctl --user -M webgate@ is-active --quiet webgate-browser.service \
  || { journalctl _UID="$U" -n 30 --no-pager; fail "the snap's browser isn't running"; }
ok "the snap runs from webgate's own systemd"

mkdir -p "$W/site"
cat > "$W/site/index.html" <<'HTML'
<!DOCTYPE html><title>Snap</title><p id=p>before</p>
<script>document.getElementById("p").textContent = "made by its script";</script>
HTML
python3 -m http.server --bind 127.0.0.1 --directory "$W/site" 8099 >/dev/null 2>&1 &
SITE=$!
trap 'kill $SITE 2>/dev/null || true' EXIT
sleep 1
python3 - <<'PY'
import http.client, time
def ask(url, headers={}, wait=90):
    c = http.client.HTTPConnection("127.0.0.1", 8280, timeout=wait)
    c.request("GET", url, headers=headers)
    r = c.getresponse()
    return r, r.read()
for i in range(20):         # the extension asks webgate once Chromium is up
    r, data = ask("http://127.0.0.1:8099/", {"X-Gateway-Snapshot": "1"})
    if r.status == 200:
        break
    time.sleep(3)
assert r.status == 200 and b"made by its script" in data, (r.status, data[:300])
print("ok: a page through the snap, its script run")
r, data = ask("http://webgate/live?url=http%3A%2F%2F127.0.0.1%3A8099%2F&w=800&h=600")
assert r.status == 200 and b"width=790" in data, (r.status, data[:300])
r, data = ask("http://webgate/live/shot.jpg?n=1")
assert r.status == 200 and data[:2] == b"\xff\xd8", (r.status, data[:200])
import struct
i = 2
while i < len(data):        # the JPEG's size, from its SOF
    m, n = data[i + 1], struct.unpack(">H", data[i + 2:i + 4])[0]
    if m in (0xC0, 0xC1, 0xC2):
        h, w = struct.unpack(">HH", data[i + 5:i + 9])
        break
    i += 2 + n
assert (w, h) == (790, 1650), (w, h)
print("ok: live view's picture at EWeb's size, 790x1650")
PY

echo "== remove web"
"$GW/aera-gateway" remove web
[ ! -e /var/lib/webgate/.config/systemd/user/webgate-browser.service ] || fail "remove left the snap's unit"
loginctl show-user webgate -p Linger 2>/dev/null | grep -qx Linger=yes && fail "webgate still lingers"
ok "web --browser with the snap"
echo "All good."
