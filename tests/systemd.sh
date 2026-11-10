#!/bin/bash
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
# aera-gateway on a real computer with systemd (CI's runner, or a spare
# Pi that runs none of the gateways yet): it takes over what the apps'
# own installers set up, with their settings; goes back to the old
# gateway when the new one can't start; installs fresh and removes; and
# its claude recipe leaves what ClaudeST's own pi/install.sh left.
#
#   sudo tests/systemd.sh            (needs the network: it fetches the apps)
set -euo pipefail
GW=$(cd "$(dirname "$0")/../gateway" && pwd)
W=$(mktemp -d)
fail() { echo "FAIL: $*" >&2; exit 1; }
ok() { echo "ok: $*"; }
running() { systemctl is-active --quiet "$1" || { journalctl -u "$1" -n 20 --no-pager; fail "$1 isn't running"; }; }
listens() { ss -ltn "( sport = :$1 )" | grep -q LISTEN || fail "nothing listens on port $1"; }
unit_has() { grep -q -- "$2" "/etc/systemd/system/$1.service" || { cat "/etc/systemd/system/$1.service"; fail "$1: no '$2'"; }; }

[ "$(id -u)" = 0 ] || fail "run it with sudo"
for repo in EMail:v1.30 ECal:v1.97 EFTP:v1.31 ClaudeST:v1.34; do
  git clone -q --depth 1 --branch "${repo#*:}" "https://github.com/whomper/${repo%:*}" "$W/${repo%:*}" 2>/dev/null
done

echo "== The apps' own installers, as a Pi has them today"
"$W/EMail/gateway/install.sh" --ports 2143,2587,2110 imap.invalid smtp.invalid:587
"$W/ECal/gateway/install.sh" --allow-local
"$W/EFTP/gateway/install.sh" --allow 127.0.0.1
mkdir -p /var/lib/eftpgate
echo "[sftp.example]:22 ssh-ed25519 AAAAC3NzaC1lZDI1NTE5AAAAIOMqqnkVzrm0SdG6UOoqKLsabgH5C9okWi0dh2l9GKJl" \
  > /var/lib/eftpgate/known_hosts
cp /var/lib/eftpgate/known_hosts "$W/known_hosts"
"$GW/aera-gateway" status

echo "== update takes them over"
"$GW/aera-gateway" update
"$GW/aera-gateway" status
for s in emailgate ecalgate eftpgate; do
  running $s
  [ ! -e /usr/local/bin/$s.py ] || fail "/usr/local/bin/$s.py is still there"
  unit_has $s "^ExecStart=/usr/bin/python3 /opt/aera-gateway/$s.py "
done
unit_has emailgate "-v --ports 2143,2587,2110 imap.invalid smtp.invalid:587$"
unit_has ecalgate "--port 8180 --allow-local$"
unit_has eftpgate "--port 2121 --pasv-ports 30000-30099 --allow 127.0.0.1$"
cmp /var/lib/eftpgate/known_hosts "$W/known_hosts" || fail "the remembered SSH keys changed"
python3 - <<'PY'
import socket, urllib.request

def first_line(port):
    with socket.create_connection(("127.0.0.1", port), timeout=30) as s:
        return s.makefile("rb").readline().decode()

line = first_line(2143)
assert line.startswith("* BYE emailgate: can't reach imap.invalid:993"), line
line = first_line(2121)
assert line.startswith("220 eftpgate 1.1:"), line
page = urllib.request.urlopen("http://127.0.0.1:8180/", timeout=30).read().decode()
assert page.startswith("ecalgate 1.1 is running"), page
print("ok: the new gateways answer")
PY

echo "== A change; and the old one back when the new one can't start"
"$GW/aera-gateway" install dav --port 8181 --no-allow-local
unit_has ecalgate "--port 8181$"
listens 8181
python3 -m http.server --bind 127.0.0.1 8182 >/dev/null 2>&1 &
BLOCKER=$!
sleep 1
if "$GW/aera-gateway" install dav --port 8182; then fail "it took a port another program has"; fi
kill $BLOCKER
unit_has ecalgate "--port 8181$"
running ecalgate
listens 8181
ok "the one before runs again"

echo "== Removing, and installing afresh"
"$GW/aera-gateway" remove ftp
[ -e /var/lib/eftpgate/known_hosts ] || fail "remove took the remembered SSH keys"
"$GW/aera-gateway" remove dav
"$GW/aera-gateway" remove mail
[ ! -e /opt/aera-gateway ] || fail "/opt/aera-gateway is left after the last one"
"$GW/aera-gateway" install mail imap.invalid smtp.invalid pop.invalid
"$GW/aera-gateway" install dav
"$GW/aera-gateway" install ftp --allow 127.0.0.1 -v
"$GW/aera-gateway" install web
for s in emailgate ecalgate eftpgate webgate; do running $s; done
for p in 143 587 110 8180 2121 8280; do listens $p; done
python3 - <<'PY'
import urllib.request
page = urllib.request.urlopen("http://127.0.0.1:8280/", timeout=30).read().decode()
assert page.startswith("webgate ") and "is running" in page, page
print("ok: webgate answers")
PY
unit_has eftpgate "--port 2121 --pasv-ports 30000-30099 --allow 127.0.0.1 -v$"
# the old way in, from an app's copy of gateway/
cp -r "$GW/." "$W/EMail/gateway/"
"$W/EMail/gateway/install.sh" --remove
[ ! -e /etc/systemd/system/emailgate.service ] || fail "EMail's install.sh --remove left emailgate"
"$GW/aera-gateway" remove dav
"$GW/aera-gateway" remove ftp
"$GW/aera-gateway" remove web
[ ! -e /opt/aera-gateway ] || fail "/opt/aera-gateway is left after the last one"
ok "fresh installs"

echo "== web --browser: the Pi's browser, out through webgate only"
# Ubuntu's Chromium is a snap: a stand-in that notes how it was started
# (EWeb's own tests drive a real one); Xvfb, xdotool and Pillow are real
cat > /usr/local/bin/chromium <<'SH'
#!/bin/sh
printf '%s\n' "$@" > /var/lib/webgate/chromium-args
exec sleep infinity
SH
chmod 755 /usr/local/bin/chromium
printf 'MemTotal:         948304 kB\n' > "$W/meminfo"
mount --bind "$W/meminfo" /proc/meminfo
if "$GW/aera-gateway" install web --browser; then umount /proc/meminfo; fail "--browser on a 1 GB Pi"; fi
umount /proc/meminfo
[ ! -e /etc/systemd/system/webgate.service ] || fail "a refused --browser left webgate"
"$GW/aera-gateway" install web --browser
sleep 3
for s in webgate webgate-screen webgate-browser; do running $s; done
listens 8280
listens 8281
unit_has webgate "--port 8280 --browser$"
unit_has webgate "^Environment=DISPLAY=:99$"
[ "$(sudo -u webgate DISPLAY=:99 xdotool getdisplaygeometry)" = "1280 3072" ] || fail "no screen for the browser"
grep -qx -- "--proxy-server=http://127.0.0.1:8281" /var/lib/webgate/chromium-args || fail "the browser doesn't go through webgate"
grep -qx -- "--load-extension=/opt/aera-gateway/webgate-ext" /var/lib/webgate/chromium-args || fail "no webgate-ext"
grep -qx -- "--host-resolver-rules=MAP \* ~NOTFOUND, EXCLUDE 127.0.0.1" /var/lib/webgate/chromium-args \
  || fail "the browser resolves names itself"
grep -qx -- "--disable-quic" /var/lib/webgate/chromium-args || fail "the browser may use QUIC"
[ -e /opt/aera-gateway/webgate-ext/manifest.json ] || fail "webgate-ext isn't in /opt"
python3 - <<'PY'
import socket, urllib.error, urllib.request
# what the browser asks of webgate: a page at home, and a tunnel to one
o = urllib.request.build_opener(urllib.request.ProxyHandler({"http": "http://127.0.0.1:8281"}))
try:
    o.open("http://127.0.0.1:8180/", timeout=30)
    raise SystemExit("the browser's proxy let a page at home through")
except urllib.error.HTTPError as e:
    assert e.code == 403 and e.read().startswith(b"webgate: won't connect to the local network"), e
s = socket.create_connection(("127.0.0.1", 8281), timeout=30)
s.sendall(b"CONNECT localhost:8180 HTTP/1.1\r\nHost: localhost\r\n\r\n")
answer = s.recv(4096)
assert answer.startswith(b"HTTP/1.1 403"), answer
print("ok: the browser's way out keeps to the home-network rule")
PY
"$GW/aera-gateway" update
unit_has webgate "--browser$"
running webgate-browser
"$GW/aera-gateway" install web --no-browser
[ ! -e /etc/systemd/system/webgate-browser.service ] || fail "--no-browser left the browser"
"$GW/aera-gateway" install web --browser
"$GW/aera-gateway" remove web
for u in webgate webgate-screen webgate-browser; do
  [ ! -e /etc/systemd/system/$u.service ] || fail "remove web left $u"
done
rm /usr/local/bin/chromium
ok "web --browser"

echo "== web: pictures as the Atari's pens, by aerapens behind its socket"
"$GW/aera-gateway" install web --allow-local
systemctl is-active --quiet webgate-pens.socket || fail "aerapens' socket isn't listening"
[ -x /opt/aera-gateway-pens/aerapens ] || fail "aerapens wasn't built"
[ -z "$(git -C "$GW/.." status --porcelain --ignored pens)" ] || fail "the build left files in the copy"
sleep 2
journalctl -u webgate --no-pager | grep -q "webgate: pictures as pens by aerapens [0-9a-f]\{12\} v" \
  || { journalctl -u webgate -n 20 --no-pager; fail "webgate didn't find aerapens"; }
mkdir -p "$W/pics"
python3 - "$W/pics" <<'PY'
import sys
from PIL import Image
d = sys.argv[1]
Image.new("RGB", (1200, 800), (200, 120, 40)).save(d + "/photo.jpg", quality=90)
Image.new("RGB", (5000, 4000), (10, 200, 90)).save(d + "/huge.png")    # 20 MP: over aerapens' 16
PY
python3 -m http.server --bind 127.0.0.1 --directory "$W/pics" 8099 >/dev/null 2>&1 &
PICS=$!
sleep 1
python3 - <<'PY'
import http.client, zlib
st16 = bytes(range(48))
def ask(method, url, headers=None, body=None):
    c = http.client.HTTPConnection("127.0.0.1", 8280, timeout=60)
    c.request(method, url, body=body, headers=headers or {})
    r = c.getresponse()
    return r, r.read()
r, _ = ask("POST", "http://webgate/palette", body=st16)
crc = r.getheader("X-Gateway-Palette")
assert r.status == 204 and crc == "%08x" % zlib.crc32(st16), (r.status, crc)
want = {"X-Gateway-Picture": "fit=600; size=300x200; pens=16; pal=%s; ed=lj" % crc}
r, data = ask("GET", "http://127.0.0.1:8099/photo.jpg", want)
assert r.getheader("Content-Type") == "application/x-aera-pens", (r.getheaders(), data[:80])
assert r.getheader("X-Gateway-Pens").startswith("aerapens "), r.getheaders()
assert data[:4] == b"EWPN" and len(data) == 20 + 150 * 200, (data[:20], len(data))
print("ok: a photo as the Atari's pens, by", r.getheader("X-Gateway-Pens"))
want["X-Gateway-Picture"] = "fit=600; pens=16; pal=%s; ed=lj" % crc
r, data = ask("GET", "http://127.0.0.1:8099/huge.png", want)
assert r.status == 200 and r.getheader("Content-Type") in ("image/jpeg", "image/png"), r.getheaders()
assert r.getheader("X-Gateway-Pens") is None
print("ok: a 20-megapixel picture beyond aerapens comes as", r.getheader("Content-Type"))
PY
kill $PICS
# aerapens' own limits, as its unit sets them
[ "$(systemctl show -p LimitAS --value 'webgate-pens@x.service')" = 67108864 ] || fail "aerapens' memory isn't limited"
[ "$(systemctl show -p PrivateNetwork --value 'webgate-pens@x.service')" = yes ] || fail "aerapens has a network"
"$GW/aera-gateway" remove web
[ ! -e /etc/systemd/system/webgate-pens.socket ] || fail "remove web left aerapens' socket"
[ ! -e /opt/aera-gateway-pens ] || fail "remove web left aerapens"
command -v cc >/dev/null || fail "remove web took gcc"
ok "web: pens"

echo "== web: scripts compiled for EWeb, by aerajsc behind its socket"
"$GW/aera-gateway" install web --allow-local
systemctl is-active --quiet webgate-js.socket || fail "aerajsc's socket isn't listening"
[ -x /opt/aera-gateway-jsc/aerajsc ] || fail "aerajsc wasn't built"
[ -z "$(git -C "$GW/.." status --porcelain --ignored jsc)" ] || fail "the build left files in the copy"
sleep 2
TAG=$(journalctl -u webgate --no-pager | sed -n 's/.*webgate: scripts compiled by aerajsc //p' | tail -1)
[ -n "$TAG" ] || { journalctl -u webgate -n 20 --no-pager; fail "webgate didn't find aerajsc"; }
mkdir -p "$W/js"
python3 -c "print('var a = /x+/.test(\'xx\');\n' * 200)" > "$W/js/a.js"
echo "var = ;" > "$W/js/bad.js"
python3 -m http.server --bind 127.0.0.1 --directory "$W/js" 8099 >/dev/null 2>&1 &
JS=$!
sleep 1
python3 - "$TAG" <<'PY'
import http.client, sys
tag = sys.argv[1]
def ask(url, headers):
    c = http.client.HTTPConnection("127.0.0.1", 8280, timeout=60)
    c.request("GET", url, headers=headers)
    r = c.getresponse()
    return r, r.read()
r, data = ask("http://127.0.0.1:8099/a.js", {"X-Gateway-Script": tag})
assert r.getheader("Content-Type") == "application/x-eweb-qjs", (r.getheaders(), data[:80])
assert data.startswith(("EWQJS %s\n" % tag).encode()), data[:80]
print("ok: a script compiled for EWeb,", len(data), "bytes")
r, data = ask("http://127.0.0.1:8099/bad.js", {"X-Gateway-Script": tag})
assert r.getheader("X-Gateway-Script") is None and data == b"var = ;\n", (r.getheaders(), data)
r, data = ask("http://127.0.0.1:8099/a.js", {"X-Gateway-Script": "another"})
assert r.getheader("X-Gateway-Script") is None and data.startswith(b"var a"), r.getheaders()
print("ok: one that doesn't compile, and one for another QuickJS, as they came")
PY
kill $JS
[ "$(systemctl show -p PrivateNetwork --value 'webgate-js@x.service')" = yes ] || fail "aerajsc has a network"
grep -qx "MaxConnections=2" /etc/systemd/system/webgate-js.socket || fail "aerajsc's socket takes more than 2 at once"
# an update with the same jsc/ keeps the aerajsc built from it
"$GW/aera-gateway" update > "$W/update.out" 2>&1 || { cat "$W/update.out"; fail "update after scripts"; }
! grep -q "Building aerajsc" "$W/update.out" || fail "the same jsc/ built again"
systemctl is-active --quiet webgate-js.socket || fail "aerajsc's socket isn't listening after the update"
ok "the same aerajsc kept"
"$GW/aera-gateway" remove web
[ ! -e /etc/systemd/system/webgate-js.socket ] || fail "remove web left aerajsc's socket"
[ ! -e /opt/aera-gateway-jsc ] || fail "remove web left aerajsc"
ok "web: scripts"

echo "== pdf: pdfgate and aerapdf, built from pdf/ (EPDF's converter)"
"$GW/aera-gateway" install pdf --allow 127.0.0.1 > "$W/pdf.out" 2>&1 || { cat "$W/pdf.out"; fail "install pdf"; }
grep -q "Building aerapdf" "$W/pdf.out" || fail "aerapdf wasn't built"
running pdfgate
listens 8380
unit_has pdfgate "^ExecStart=/usr/bin/python3 /opt/aera-gateway/pdfgate.py --port 8380 --allow 127.0.0.1$"
unit_has pdfgate "^User=pdfgate"
[ -x /opt/aera-gateway-pdf/aerapdf ] || fail "no aerapdf in /opt/aera-gateway-pdf"
[ -z "$(git -C "$GW/.." status --porcelain --ignored pdf)" ] || fail "the build left files in the copy"
python3 "$GW/../tests/pdf/small-pdf.py" "$W/Wilhelmina.pdf" 3
/opt/aera-gateway-pdf/aerapdf --fonts /opt/aera-gateway-pdf/fonts "$W/Wilhelmina.pdf" "$W/direct.pdf" > /dev/null
python3 - "$W" <<'PY'
import socket, sys
w = sys.argv[1]
def ask(req):
    s = socket.create_connection(("127.0.0.1", 8380), timeout=120)
    s.sendall(req)
    out = b""
    while True:
        b = s.recv(65536)
        if not b:
            return out
        out += b
r = ask(b"GET /version HTTP/1.0\r\n\r\n")
assert b"\r\n\r\naerapdf " in r, r
pdf = open(w + "/Wilhelmina.pdf", "rb").read()
r = ask(b"POST /convert?name=Wilhelmina.pdf HTTP/1.1\r\nHost: x\r\nContent-Length: %d\r\n\r\n" % len(pdf) + pdf)
at = r.rfind(b"HTTP/1.1 200")
assert at >= 0, r[:300]
head, _, body = r[at:].partition(b"\r\n\r\n")
assert body == open(w + "/direct.pdf", "rb").read(), (head, len(body))
r = ask(b"POST /convert HTTP/1.1\r\nHost: x\r\nContent-Length: 4\r\n\r\nnope")
assert r.startswith(b"HTTP/1.1 415") and r.endswith(b"pdfgate: not a PDF\n"), r
print("ok: /version, a PDF converted byte for byte as aerapdf does directly, a refusal")
PY
sleep 1
! journalctl -u pdfgate --no-pager | grep -qi "wilhelmina" || fail "the PDF's name is in the journal"
journalctl -u pdfgate --no-pager | grep -q "127.0.0.1 200, " || fail "no log line for the conversion"
"$GW/aera-gateway" update > "$W/update.out" 2>&1 || { cat "$W/update.out"; fail "update with pdf"; }
! grep -q "Building aerapdf" "$W/update.out" || fail "the same pdf/ built again"
running pdfgate
id -nG pdfgate | grep -qw aera-translate || fail "pdfgate isn't in aera-translate"
ok "pdf (removed after translate, which it uses)"

echo "== translate: translategate with the stand-in engine (no model in CI)"
"$GW/aera-gateway" install translate --engine stand-in --allow 127.0.0.1
running translategate
listens 8480
unit_has translategate "^ExecStart=/usr/bin/python3 /opt/aera-gateway/translategate.py --port 8480 --allow 127.0.0.1 --local /run/translategate/local.sock$"
unit_has translategate "^User=translategate"
unit_has translategate "^MemoryMax=1G"
unit_has translategate "^OOMScoreAdjust=-300"
[ ! -e /opt/translategate/venv ] || fail "the stand-in set up an engine"
# the local socket, for webgate and pdfgate: the group's only
[ "$(stat -c '%a %U %G' /run/translategate/local.sock)" = "660 translategate aera-translate" ] || \
  { stat /run/translategate/local.sock; fail "the local socket isn't translategate's and its group's only"; }
[ "$(stat -c '%a' /run/translategate)" = 750 ] || fail "/run/translategate is open to others"
python3 - "$GW" <<'PY'
import sys
sys.path.insert(0, sys.argv[1])
import translateclient as tc
text, used, same = tc.translate("Hallo Wilhelmina, das ist gut.", "en")
assert (text, used, same) == ("en: HALLO WILHELMINA, DAS IST GUT.", "de", False), (text, used, same)
print("ok: the local socket, as webgate and pdfgate use it")
PY
# pdfgate's translate=CODE: aerapdf runs translateclient as pdfgate's user,
# which reaches the local socket through its group (joined at its install)
python3 - "$W" <<'PY'
import socket, sys, zlib, re
w = sys.argv[1]
pdf = open(w + "/Wilhelmina.pdf", "rb").read()
s = socket.create_connection(("127.0.0.1", 8380), timeout=120)
s.sendall(b"POST /convert?translate=de&name=Wilhelmina.pdf HTTP/1.1\r\nHost: x\r\nContent-Length: %d\r\n\r\n"
          % len(pdf) + pdf)
r = b""
while True:
    b = s.recv(65536)
    if not b:
        break
    r += b
at = r.rfind(b"HTTP/1.1 200")
assert at >= 0, r[-300:]
head, _, body = r[at:].partition(b"\r\n\r\n")
assert b"\r\nX-Translate-From: en\r\n" in head, head
assert body.count(b"/Translation ") == 3, body.count(b"/Translation ")
assert body != open(w + "/direct.pdf", "rb").read()
print("ok: a PDF converted and translated (translate=de), through translategate's local socket")
PY
"$GW/aera-gateway" remove pdf
[ ! -e /etc/systemd/system/pdfgate.service ] || fail "remove pdf left its unit"
[ ! -e /opt/aera-gateway-pdf ] || fail "remove pdf left aerapdf"
[ -e /opt/aera-gateway/translate-phrases.tsv ] || fail "translate-phrases.tsv isn't in /opt"
python3 - <<'PY'
import socket
def ask(req):
    s = socket.create_connection(("127.0.0.1", 8480), timeout=20)
    s.sendall(req)
    out = b""
    while True:
        b = s.recv(65536)
        if not b:
            return out
        out += b
r = ask(b"GET /status HTTP/1.0\r\n\r\n")
assert r.endswith(b"\r\n\r\ntranslategate 1\nengine stand-in 1\nready\n"), r
body = "Dear Wilhelmina,\nthe meeting moved to 10:30, see https://example.com/m.\n\nBye".encode()
r = ask(b"POST /translate?to=en&from=de HTTP/1.0\r\nContent-Length: %d\r\n\r\n" % len(body) + body)
head, _, text = r.partition(b"\r\n\r\n")
assert head.startswith(b"HTTP/1.0 200") and b"X-Translate-From: de" in head, head
assert text == "en: DEAR WILHELMINA, THE MEETING MOVED TO 10:30, SEE https://example.com/m.\n\nen: BYE".encode(), text
r = ask(b"POST /translate?to=en HTTP/1.0\r\nContent-Length: 9000\r\n\r\n")
assert r.startswith(b"HTTP/1.0 413") and r.endswith(b"translategate: the text is over 8 KB\n"), r
print("ok: status, a letter's lines joined and a URL kept, a refusal")
PY
sleep 1
! journalctl -u translategate --no-pager | grep -qi "wilhelmina" || fail "a word of the text is in the journal"
journalctl -u translategate --no-pager | grep -q "127.0.0.1 de>en 200, " || fail "no log line for the translation"
# the Languages page: a request, the job (with an index and models of the
# test's own, as file: URLs), the models in place, translategate again
systemctl is-active --quiet translategate-langs.path || fail "the languages' job isn't watching"
mkdir -p /srv/tl-fake /etc/systemd/system/translategate-langs.service.d
python3 - "$GW" <<'PY'
import sys
sys.path.insert(0, sys.argv[1])
from test_translatelangs import fake_index
print(fake_index("/srv/tl-fake", [("he", "en"), ("en", "he")]))
PY
chmod -R a+rX /srv/tl-fake
printf '[Service]\nEnvironment=TRANSLATELANGS_INDEX=file:///srv/tl-fake/index.json\n' \
  > /etc/systemd/system/translategate-langs.service.d/test.conf
systemctl daemon-reload
python3 - <<'PY'
import socket, time
def ask(req):
    s = socket.create_connection(("127.0.0.1", 8480), timeout=20)
    s.sendall(req)
    out = b""
    while True:
        b = s.recv(65536)
        if not b:
            return out.partition(b"\r\n\r\n")
        out += b
head, _, body = ask(b"POST /languages HTTP/1.0\r\nContent-Length: 2\r\n\r\nhe")
assert head.startswith(b"HTTP/1.0 202") and body == b"job 1\n", (head, body)
for _ in range(60):
    time.sleep(1)
    try:
        _, _, body = ask(b"GET /job HTTP/1.0\r\n\r\n")
    except OSError:
        continue                        # (translategate starting again)
    if not body.startswith(b"job 1\nrunning"):
        break
assert body == b"job 1\ndone\n", body
for _ in range(20):
    time.sleep(1)
    try:
        _, _, body = ask(b"GET /languages?all=1 HTTP/1.0\r\n\r\n")
        break
    except OSError:
        pass
assert b"\nhe\tHebrew\t230\tinstalled\n" in body and b"used_mb 230\n" in body, body
print("ok: the Languages page's request, its job and the models in place")
PY
[ -e /opt/translategate/models/he_en/model/model.bin ] || fail "the job didn't put he>en in place"
[ "$(stat -c %U /opt/translategate/models/he_en)" = translategate ] || fail "the job's models aren't translategate's"
journalctl -u translategate --no-pager | grep -q "starting again with them" || fail "translategate didn't start again"
rm -rf /etc/systemd/system/translategate-langs.service.d /srv/tl-fake
systemctl daemon-reload
"$GW/aera-gateway" update > "$W/update.out" 2>&1 || { cat "$W/update.out"; fail "update with translate"; }
unit_has translategate "translategate.py --port 8480 --allow 127.0.0.1 --local /run/translategate/local.sock$"
running translategate
"$GW/aera-gateway" remove translate
[ ! -e /etc/systemd/system/translategate.service ] || fail "remove translate left its unit"
[ ! -e /etc/systemd/system/translategate-langs.path ] || fail "remove translate left the languages' job"
[ ! -e /opt/translategate ] || fail "remove translate left /opt/translategate"
ok "translate"

echo "== radio: radiogate (EPlay's web radio), ffmpeg for what isn't MP3"
"$GW/aera-gateway" install radio --allow 127.0.0.1 --allow-local > "$W/radio.out" 2>&1 || \
  { cat "$W/radio.out"; fail "install radio"; }
running radiogate
listens 8580
unit_has radiogate "^ExecStart=/usr/bin/python3 /opt/aera-gateway/radiogate.py --port 8580 --allow 127.0.0.1 --allow-local$"
unit_has radiogate "^User=radiogate"
unit_has radiogate "^MemoryMax=768M"
command -v ffmpeg >/dev/null || fail "install radio didn't put ffmpeg in"
mkdir -p "$W/Wilhelmina"
ffmpeg -hide_banner -loglevel error -f lavfi -i sine=frequency=440:duration=8 -ac 2 -b:a 128k "$W/Wilhelmina/a.mp3"
ffmpeg -hide_banner -loglevel error -f lavfi -i sine=frequency=440:duration=8 -ac 2 -c:a libvorbis "$W/Wilhelmina/a.ogg"
ffmpeg -hide_banner -loglevel error -f lavfi -i color=red:s=300x200 -frames:v 1 "$W/Wilhelmina/cover.png"
(cd "$W" && exec python3 -m http.server 8590 --bind 127.0.0.1 > /dev/null 2>&1) &
station=$!
sleep 1
python3 - "$W" <<'PY'
import socket, sys
w = sys.argv[1]
def ask(path):
    s = socket.create_connection(("127.0.0.1", 8580), timeout=60)
    s.sendall(b"GET %s HTTP/1.0\r\n\r\n" % path)
    out = b""
    while True:
        b = s.recv(65536)
        if not b:
            return out
        out += b
r = ask(b"/version")
assert b"\r\n\r\nradiogate 1.3, ffmpeg " in r and not r.endswith(b"ffmpeg none\n"), r
head, _, body = ask(b"/stream?url=http%3A%2F%2F127.0.0.1%3A8590%2FWilhelmina%2Fa.mp3").partition(b"\r\n\r\n")
assert head.startswith(b"HTTP/1.0 200") and b"Content-Type: audio/mpeg" in head, head
assert body == open(w + "/Wilhelmina/a.mp3", "rb").read(), len(body)
head, _, body = ask(b"/stream?url=http%3A%2F%2F127.0.0.1%3A8590%2FWilhelmina%2Fa.ogg").partition(b"\r\n\r\n")
assert head.startswith(b"HTTP/1.0 200") and b"Content-Type: audio/mpeg" in head, head
assert len(body) > 50000 and (body[:3] == b"ID3" or body[0] == 0xFF), (len(body), body[:4])
head, _, body = ask(b"/stream?url=file%3A%2F%2F%2Fetc%2Fpasswd").partition(b"\r\n\r\n")
assert head.startswith(b"HTTP/1.0 400") and body.startswith(b"radiogate: "), (head, body)
head, _, body = ask(b"/art?url=http%3A%2F%2F127.0.0.1%3A8590%2FWilhelmina%2Fcover.png").partition(b"\r\n\r\n")
assert head.startswith(b"HTTP/1.0 200") and b"Content-Type: image/jpeg" in head and body[:2] == b"\xff\xd8", head
print("ok: /version, an MP3 station as it came, an Ogg one converted by ffmpeg, a cover as a 64x64 JPEG, a refusal")
PY
kill $station
sleep 1
! journalctl -u radiogate --no-pager | grep -qi "wilhelmina" || fail "a station's path is in the journal"
journalctl -u radiogate --no-pager | grep -q "127.0.0.1 listened to 127.0.0.1 for " || fail "no log line for a station"
"$GW/aera-gateway" update > "$W/update.out" 2>&1 || { cat "$W/update.out"; fail "update with radio"; }
unit_has radiogate "radiogate.py --port 8580 --allow 127.0.0.1 --allow-local$"
running radiogate
"$GW/aera-gateway" remove radio
[ ! -e /etc/systemd/system/radiogate.service ] || fail "remove radio left its unit"
ok "radio"

echo "== dash: dashgate, the dashboard; the gateways tell it through aeragate"
"$GW/aera-gateway" install dash > "$W/dash.out" 2>&1 || { cat "$W/dash.out"; fail "install dash"; }
running dashgate
listens 8888
unit_has dashgate "^ExecStart=/usr/bin/python3 /opt/aera-gateway/dashgate.py --port 8888$"
unit_has dashgate "^User=dashgate"
unit_has dashgate "^RuntimeDirectory=aera-dash"
[ -S /run/aera-dash/events.sock ] || fail "no /run/aera-dash/events.sock"
systemctl is-active --quiet ecalgate || "$GW/aera-gateway" install dav > /dev/null
python3 - <<'PY'
import json, time, urllib.request
urllib.request.urlopen("http://127.0.0.1:8180/", timeout=30).read()
for _ in range(20):
    d = json.loads(urllib.request.urlopen("http://127.0.0.1:8888/status.json", timeout=30).read())
    svc = {s["key"]: s for s in d["services"]}
    if svc["dav"]["total"] >= 1:
        break
    time.sleep(0.5)
assert svc["dav"]["state"] == "running" and svc["dav"]["since"], svc["dav"]
assert svc["dav"]["total"] >= 1, svc["dav"]          # told by ecalgate, in its own sandbox
assert svc["radio"]["state"] == "not installed", svc["radio"]
assert d["gateway"]["where"] == "systemd" and d["gateway"]["version"], d["gateway"]
assert d["ataris"] and d["ataris"][0]["ip"] == "127.0.0.1", d["ataris"]
page = urllib.request.urlopen("http://127.0.0.1:8888/", timeout=30)
assert b"AERA-Gateway" in page.read() and "script-src 'self'" in page.headers["Content-Security-Policy"]
print("ok: the page, each service's state from systemd, a dav connection counted")
PY
"$GW/aera-gateway" update > "$W/update.out" 2>&1 || { cat "$W/update.out"; fail "update with dash"; }
unit_has dashgate "dashgate.py --port 8888$"
running dashgate
"$GW/aera-gateway" remove dash
[ ! -e /etc/systemd/system/dashgate.service ] || fail "remove dash left its unit"
ok "dash"

echo "== claude: the recipe leaves what ClaudeST's pi/install.sh left, then takes AERA-Gateway's own bridge"
snap() {
  mkdir -p "$1"
  grep -v '^#' /etc/claude-st/claude-st.env > "$1/env"    # the settings; the notes name today's commands
  stat -c '%a %U:%G' /etc/claude-st/claude-st.env > "$1/env.stat"
  cp /etc/systemd/system/claude-st.service "$1/unit"
  stat -c '%a' /etc/systemd/system/claude-st.service > "$1/unit.stat"
  (cd /opt/claude-st/bridge && for f in *; do echo "$f $(stat -c %a "$f") $(md5sum < "$f")"; done) > "$1/bridge"
  getent passwd claude-st | cut -d: -f6,7 > "$1/user"
  id -nG claude-st | tr ' ' '\n' | sort > "$1/groups"
}
# pi/install.sh 1.34 stops quietly on a Pi without --allow in its settings
# (a new one, serial, --atari any): grep finds nothing and pipefail ends it
sed -i "s/^cur_atari=\$(grep -o -- '--allow \[^ \]\*' <<<\"\$args\"/cur_atari=\$( (grep -o -- '--allow [^ ]*' <<<\"\$args\" || true)/" \
  "$W/ClaudeST/pi/install.sh"
grep -q "|| true) | awk" "$W/ClaudeST/pi/install.sh" || fail "can't mend pi/install.sh's --allow line"
"$W/ClaudeST/pi/install.sh" --network --atari 192.168.1.20 --backend demo < /dev/null
snap "$W/old"
"$W/ClaudeST/pi/install.sh" --uninstall
rm -rf /etc/claude-st /var/lib/claude-st
userdel claude-st
"$GW/aera-gateway" install claude --from "$W/ClaudeST" --network --atari 192.168.1.20 --backend demo < /dev/null
snap "$W/new"
diff -r "$W/old" "$W/new" || fail "the claude recipe differs from pi/install.sh"
sleep 3
running claude-st
listens 2323
sed -i 's/^CLAUDE_SESSION_KEY=$/CLAUDE_SESSION_KEY=sk-test-not-a-key/' /etc/claude-st/claude-st.env
# an update takes the bridge that comes with AERA-Gateway (claude-st/, moved
# from ClaudeST on 9 Nov 2026), every setting and key kept
"$GW/aera-gateway" update < /dev/null
grep -q "^CLAUDE_SESSION_KEY=sk-test-not-a-key$" /etc/claude-st/claude-st.env || fail "update lost the key"
grep -q "^CLAUDE_ST_ARGS=--backend demo --tcp 0.0.0.0:2323 --allow 192.168.1.20$" /etc/claude-st/claude-st.env \
  || fail "update changed the settings"
for f in claude_bridge.py backends.py atari_text.py requirements.txt; do
  cmp -s "$GW/claude-st/bridge/$f" "/opt/claude-st/bridge/$f" || fail "update didn't put AERA-Gateway's $f in place"
done
[ ! -e /opt/claude-st/bridge/test_bridge.py ] || fail "the bridge's tests were installed"
cmp -s "$GW/claude-st/claude-st.service" /etc/systemd/system/claude-st.service || fail "the unit isn't AERA-Gateway's"
grep -q "^AERA-Gateway " /opt/claude-st/SOURCE || fail "SOURCE doesn't name AERA-Gateway"
sleep 3
running claude-st
listens 2323
"$GW/aera-gateway" remove claude
[ -e /etc/claude-st/claude-st.env ] || fail "remove took ClaudeST's settings"
[ ! -e /opt/claude-st ] || fail "remove left /opt/claude-st"
ok "claude"
echo "All good."
