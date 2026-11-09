# Working on AERA-Gateway

What is where in this repository, how the apps carry the gateways, how a
release is made, and the tests. Using the Gateway: the
[README](../README.md); each service in full: [SERVICES.md](SERVICES.md).

## Source

```
gateway/      what runs on the Gateway; the apps carry a copy of this folder
  aera-gateway    the installer: install (one service, or all), update, status, remove, set-key
  aeragate.py     the rules every gateway keeps: who may use it, where it may go, TLS, the log
  emailgate.py    mail: plain IMAP, SMTP and POP3 in, TLS out
  ecalgate.py     dav: plain HTTP in proxy form in, HTTPS out, feeds trimmed
  eftpgate.py     ftp: plain FTP in, FTPS or SFTP out
  webgate.py      web: plain HTTP in proxy form in, HTTP or HTTPS out
  webgate-ext/    webgate's extension for the Gateway's browser (install web --browser)
  pdfgate.py      pdf: PDFs converted by aerapdf, EPDF's converter
  translategate.py translate: text translated on the Gateway by a local engine (Argos Translate)
  translate-phrases.tsv translategate's greetings and closings, a plain list
  translatelangs.py translate: the languages installed and removed (the apps' Languages page)
  translateclient.py translate: translategate's local socket, for webgate and pdfgate (aerapdf's translator)
  translate-languages.tsv the languages that can be installed, with their sizes
  radiogate.py    radio: web radio (MP3, AAC, Ogg, HLS) as MP3 for EPlay (EPlay's radio/, tools/sync-radio.sh; ffmpeg)
  EPLAY           the EPlay commit radiogate.py is from
  claude-st/      claude: ClaudeST's bridge (bridge/, its unit; CLAUDEST: the ClaudeST commit it came from)
  install.sh      the apps' old way in, handed on to aera-gateway
  test_*.py       their tests
pdf/              aerapdf, pdfgate's converter: built on the Gateway by install pdf
  epdf/           its source, EPDF's converter at the commit in pdf/EPDF (tools/sync-pdf.sh)
pens/             aerapens, for webgate's pictures as the Atari's pens: built on the Gateway by install web
  common/         its source, AERA-Atari's common/ at the commit in pens/COMMON (tools/sync-common.sh)
jsc/              aerajsc, for webgate's scripts compiled for EWeb: built on the Gateway by install web
  eweb/           its source, EWeb's tools/jsc and QuickJS at the commit in jsc/EWEB (tools/sync-jsc.sh)
tools/
  sync-common.sh  pens/common/ from AERA-Atari; never edit pens/common/ here
  sync-jsc.sh     jsc/eweb/ from EWeb; never edit jsc/eweb/ here
  sync-pdf.sh     pdf/epdf/ from EPDF; never edit pdf/epdf/ here
  sync-radio.sh   gateway/radiogate.py from EPlay; never edit it here
  translate-check.py translategate's engine on a Gateway: quality, speed and memory
docs/
  SERVICES.md     the services' reference: requests, answers, refusals, limits, logs
  DOCKER.md       the Gateway in Docker: which systems, how
  DEVELOPING.md   this: the source, the apps' copies, releases, tests
docker/           the Docker image: its Dockerfile, aera-gateway-run (what systemd does on a Pi), compose files
tests/translate/  the fixed Hebrew/English set translate-check.py uses (no private text)
update-pi.sh      this copy at the latest release, then aera-gateway update (with --allow: install all)
tests/systemd.sh  aera-gateway for real, on a computer with systemd
tests/snap.sh     install web --browser for real with Ubuntu's Chromium snap
tests/docker.sh   the Docker image for real: every service, /data kept
```

ClaudeST's bridge came here from ClaudeST on 9 Nov 2026 (Erez), in
`gateway/claude-st/`: it is released with AERA-Gateway, and ClaudeST still
owns its protocol and reviews its changes.

## The apps' copies

EMail, ECal, EContact, EFTP, EWeb, EPDF, EPlay and ClaudeST carry `gateway/` as a
read-only copy (a git subtree of this repository's `gateway` branch,
brought up to date with their `tools/sync-gateway.sh`), so their tests
run against the gateway they were made with. Change the gateways here, never in a copy.
The `gateway` branch is `git subtree split --prefix=gateway` of `main`.

## Releases

The Gateway gets formal releases only, vX.Y, made from `main` by the Release
workflow (Actions > Release > Run workflow, with the notes): it reads
`VERSION` in `gateway/aera-gateway`, runs CI's tests, and tags `main`. A
release that doesn't work is never deleted or reused: revert on `main`,
raise `VERSION`, release the next number, and say in the bad release's
notes that it is withdrawn and which one to use.

## Tests

```
pip install paramiko pyftpdlib pyopenssl pillow
python3 -m unittest discover -s gateway -p 'test_*.py'
sudo tests/systemd.sh          on a spare computer with systemd: it installs and removes services
sudo tests/snap.sh             on a spare Ubuntu computer: the Gateway's browser with the Chromium snap
tests/docker.sh                on a spare computer with Docker: the image, every service in it
```

CI runs all four on every push, the first with Python 3.9, 3.11, 3.12 and
3.13 (Raspberry Pi OS bullseye, bookworm and trixie, Ubuntu 24.04).
