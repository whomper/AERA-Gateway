#!/bin/bash
# Installs / updates troll-proxy (IMAP for TROLL on port 1143) from the
# files in this folder. TROLL's SMTP (Postfix, port 25) is set up separately on the
# Pi. Ports 143 and 587 are left for mail-proxy (EMAIL.PRG).
set -e
cd "$(dirname "$0")"
for f in falcon_imap_logproxy.py falcon-imap-logproxy.service hebrew_words.txt; do
  [ -f "$f" ] || { echo "MISSING FILE: $f (copy all files first)"; exit 1; }
done
python3 falcon_imap_logproxy.py --version
# listen on the Pi's home-network address only (the one its default route
# uses); LISTEN=a.b.c.d overrides it
LISTEN=${LISTEN:-$(ip -4 route get 1.1.1.1 2>/dev/null | awk '{for (i = 1; i < NF; i++) if ($i == "src") print $(i + 1)}')}
[ -n "$LISTEN" ] || { echo "Can't find this Pi's network address; run with LISTEN=a.b.c.d"; exit 1; }
echo "--- troll-proxy will listen on $LISTEN:1143"
sudo install -d -m 755 /opt/falcon-imap
sudo install -m 644 falcon_imap_logproxy.py /opt/falcon-imap/
# never overwrite a word list you have edited
[ -f /opt/falcon-imap/hebrew_words.txt ] || sudo install -m 644 hebrew_words.txt /opt/falcon-imap/
sed "s/@LISTEN@/$LISTEN/" falcon-imap-logproxy.service |
  sudo tee /etc/systemd/system/falcon-imap-logproxy.service >/dev/null
sudo systemctl daemon-reload
sudo systemctl enable falcon-imap-logproxy.service >/dev/null 2>&1
sudo systemctl restart falcon-imap-logproxy.service

sleep 3
echo "--- service state: $(systemctl is-active falcon-imap-logproxy.service)"
echo "--- last log lines:"
sudo journalctl -u falcon-imap-logproxy.service -n 4 --no-pager | cut -c1-220
echo "--- listening ports (expect python3 on :1143; :25 is TROLL's SMTP;"
echo "    :143 and :587 are mail-proxy's, if it is installed):"
sudo ss -ltnp | grep -E ':(25|143|1143|587)\b' || echo "(nothing listening!)"
