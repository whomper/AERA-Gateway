#!/bin/bash
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
#
# The Gateway's Docker image for real: every service started by
# aera-gateway-run in one container on the host's network, each answering
# on its port, what is kept surviving a new container, and nothing of a
# key in the log. Translate runs its stand-in engine (no models in CI),
# ClaudeST its demo backend, mail towards servers that don't exist.
#
#   tests/docker.sh [IMAGE]       (default: built here, aera-gateway:test)
#
# On a spare computer with Docker and nothing on the services' ports.
set -euo pipefail
cd "$(dirname "$0")/.."
IMAGE=${1:-}
NAME=aera-gateway-test
VOL=aera-gateway-test-data
fail() { echo "FAIL: $*" >&2; docker logs "$NAME" 2>&1 | tail -40 >&2 || true; exit 1; }
ok() { echo "ok: $*"; }
cleanup() { docker rm -f "$NAME" >/dev/null 2>&1 || true; docker volume rm "$VOL" >/dev/null 2>&1 || true; }
trap cleanup EXIT
cleanup

if [ -z "$IMAGE" ]; then
  IMAGE=aera-gateway:test
  docker build -q -f docker/Dockerfile -t "$IMAGE" . >/dev/null
fi

start() {
  docker run -d --name "$NAME" --network host -v "$VOL:/data" \
    -e AERA_SERVICES=all -e AERA_ALLOW=127.0.0.1 -e AERA_MAIL="imap.invalid smtp.invalid:587" \
    -e AERA_LANGUAGES=none -e AERA_ARGS_TRANSLATE="--engine stand-in" \
    -e CLAUDE_BACKEND=demo -e CLAUDE_SESSION_KEY=sk-test-Wilhelmina \
    "$IMAGE" >/dev/null
}
waitport() {
  for _ in $(seq 1 60); do
    (exec 3<>"/dev/tcp/127.0.0.1/$1") 2>/dev/null && return 0
    sleep 1
  done
  fail "nothing answers on port $1"
}

echo "== the services, each on its port"
start
for p in 143 587 8180 2121 8280 8380 8480 8580 2323 8888; do waitport "$p"; done
ok "mail, dav, ftp, web, pdf, translate, radio, claude and the dashboard answer"

python3 - <<'PY'
import socket, urllib.request
def get(url, **h):
    req = urllib.request.Request(url, headers=h)
    with urllib.request.urlopen(req, timeout=20) as r:
        return r.status, dict(r.getheaders()), r.read()
st, _, body = get("http://127.0.0.1:8180/")
assert st == 200 and b"ecalgate" in body, body
st, _, body = get("http://127.0.0.1:8380/version")
assert body.startswith(b"aerapdf "), body
st, _, body = get("http://127.0.0.1:8480/status")
assert body.startswith(b"translategate 1\nengine stand-in"), body
st, _, body = get("http://127.0.0.1:8580/version")
assert body.startswith(b"radiogate ") and b"ffmpeg none" not in body, body
# webgate's own pages; aerajsc answers through the container's own inetd
proxy = urllib.request.ProxyHandler({"http": "http://127.0.0.1:8280"})
op = urllib.request.build_opener(proxy)
with op.open("http://webgate/", timeout=20) as r:
    assert r.headers.get("X-Gateway-Translate") == "1"
with op.open("http://webgate/scripts", timeout=20) as r:
    assert r.read().startswith(b"aerajsc "), "no aerajsc"
s = socket.create_connection(("127.0.0.1", 2121), timeout=10)
assert s.recv(200).startswith(b"220 eftpgate"), "no eftpgate banner"
s.close()
print("ok: dav, pdf, translate, radio, web (with aerajsc) and ftp answer as they should")
# the dashboard: each service running, and the connections above counted
import json, time
for _ in range(20):
    st, _, body = get("http://127.0.0.1:8888/status.json")
    d = json.loads(body)
    svc = {s["key"]: s for s in d["services"]}
    if svc["dav"]["total"] >= 1 and svc["web"]["total"] >= 1:
        break
    time.sleep(1)
assert d["gateway"]["where"] == "container" and d["gateway"]["version"], d["gateway"]
assert all(svc[k]["state"] == "running" for k in ("mail", "dav", "ftp", "web", "pdf", "translate", "radio",
                                                  "claude")), [(k, s["state"]) for k, s in svc.items()]
assert svc["dav"]["total"] >= 1 and svc["web"]["total"] >= 1 and svc["ftp"]["total"] >= 1, \
    [(k, s["total"]) for k, s in svc.items()]
assert d["ataris"][0]["ip"] == "127.0.0.1", d["ataris"]
assert "imap.invalid" in d["settings"]["mail"], d["settings"]
st, h, body = get("http://127.0.0.1:8888/")
assert b"AERA-Gateway" in body and "script-src 'self'" in h["Content-Security-Policy"]
print("ok: the dashboard: every service running, the connections counted, the settings")
PY

docker exec -i "$NAME" setpriv --reuid pdfgate --regid pdfgate --init-groups -- \
  /opt/aera-gateway/translateclient.py --to en <<<"Die Katze ist nicht da, und der Hund ist gut." | grep -q "en: DIE KATZE" \
  || fail "pdfgate's user can't use translategate's local socket"
ok "translategate's local socket, for pdfgate and webgate"

docker exec "$NAME" python3 -c '
import os, pwd
for p in os.listdir("/proc"):
    try:
        args = open("/proc/%s/cmdline" % p, "rb").read().split(b"\0")
        uid = [l.split()[1] for l in open("/proc/%s/status" % p) if l.startswith("Uid:")][0]
    except (OSError, IndexError):
        continue
    if len(args) > 1 and args[0].endswith((b"python3", b"python")):
        print(pwd.getpwuid(int(uid)).pw_name, b" ".join(args).decode())
' > /tmp/aera-ps.txt
grep -q '^emailgate .*emailgate.py' /tmp/aera-ps.txt || fail "emailgate isn't its own user"
grep -q '^webgate .*webgate.py' /tmp/aera-ps.txt || fail "webgate isn't its own user"
grep -q '^dashgate .*dashgate.py' /tmp/aera-ps.txt || fail "dashgate isn't its own user"
grep -q '^claude-st .*claude_bridge.py' /tmp/aera-ps.txt || fail "the bridge isn't claude-st's"
! grep -q '^root .*/opt/' /tmp/aera-ps.txt || fail "a service runs as root"
ok "each service as its own user"

docker exec "$NAME" aera-gateway-run status | grep -q "in a container" || fail "status"
! docker logs "$NAME" 2>&1 | grep -q Wilhelmina || fail "the session key is in the log"
docker exec "$NAME" grep -q "^CLAUDE_SESSION_KEY=sk-test-Wilhelmina$" /etc/claude-st/claude-st.env \
  || fail "the session key wasn't kept"
docker exec "$NAME" stat -c '%a %U:%G' /data/claude-st/etc/claude-st.env | grep -q "^640 root:claude-st$" \
  || fail "ClaudeST's settings are open to others"
ok "ClaudeST's key kept in /data, never in the log"

echo "== a new container keeps /data"
docker exec "$NAME" sh -c 'echo "[sftp.example]:22 ssh-ed25519 AAAA" > /var/lib/eftpgate/known_hosts'
docker rm -f "$NAME" >/dev/null
start
waitport 2121
docker exec "$NAME" grep -q "sftp.example" /var/lib/eftpgate/known_hosts || fail "known_hosts was lost"
docker exec "$NAME" grep -q "^CLAUDE_SESSION_KEY=sk-test-Wilhelmina$" /etc/claude-st/claude-st.env \
  || fail "the key was lost"
ok "the SSH host keys and ClaudeST's key kept"

echo "== a service that stops starts again"
docker exec "$NAME" python3 -c '
import os, signal
for p in os.listdir("/proc"):
    try:
        if b"radiogate.py" in open("/proc/%s/cmdline" % p, "rb").read() and p != str(os.getpid()):
            os.kill(int(p), signal.SIGKILL)
    except (OSError, ValueError):
        pass
'
sleep 8
waitport 8580
ok "radiogate started again"

docker stop -t 20 "$NAME" >/dev/null
docker logs "$NAME" 2>&1 | grep -q "aera-gateway-run: stopped" || fail "it didn't stop cleanly"
ok "stopped cleanly"
echo "All good."
