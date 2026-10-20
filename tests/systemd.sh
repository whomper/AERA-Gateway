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
[ "$(sudo -u webgate DISPLAY=:99 xdotool getdisplaygeometry)" = "1024 768" ] || fail "no screen for the browser"
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

echo "== claude: the recipe leaves what ClaudeST's pi/install.sh left"
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
"$GW/aera-gateway" update --from "$W/ClaudeST" < /dev/null
grep -q "^CLAUDE_SESSION_KEY=sk-test-not-a-key$" /etc/claude-st/claude-st.env || fail "update lost the key"
grep -q "^CLAUDE_ST_ARGS=--backend demo --tcp 0.0.0.0:2323 --allow 192.168.1.20$" /etc/claude-st/claude-st.env \
  || fail "update changed the settings"
"$GW/aera-gateway" remove claude
[ -e /etc/claude-st/claude-st.env ] || fail "remove took ClaudeST's settings"
[ ! -e /opt/claude-st ] || fail "remove left /opt/claude-st"
ok "claude"
echo "All good."
