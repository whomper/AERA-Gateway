# Notes for working on AERA-Gateway

AERA-Gateway is the Raspberry Pi side of the AERA Atari apps: the gateways
for EMail (`mail`, emailgate), ECal and EContact (`dav`, ecalgate) and EFTP
(`ftp`, eftpgate), the shared rules (`gateway/aeragate.py`) and the one
installer (`gateway/aera-gateway`), which also sets up ClaudeST's bridge
(`claude`).

## Who decides what

- This repository has its own owner session: the installer, the port
  table, `aeragate.py`, CI and the docs.
- Each app's session owns its gateway's protocol (emailgate's STARTTLS
  and "emailgate:" refusal lines, ecalgate's feed trimming and texts,
  eftpgate's FTP server, FTPS session reuse, SFTP and known_hosts, the
  bridge in ClaudeST): ask it before changing those, and send it the
  change to review.
- **A port never moves without the owning app's OK**: the Ataris have
  them in their settings (143/587/110, 8180, 2121 and 30000-30099, 2323).
- ClaudeST's bridge stays in ClaudeST; the `claude` recipe sets it up as
  ClaudeST's `pi/install.sh` did, and `tests/systemd.sh` checks that.

## Rules

- **The apps carry `gateway/` as a read-only copy**: a subtree of the
  `gateway` branch, which is `git subtree split --prefix=gateway main`.
  After a change here: push main, then
  `git subtree split --prefix=gateway -b gateway && git push origin gateway`,
  and tell the app sessions to run `tools/sync-gateway.sh`.
- **Standard Python 3 only** (3.9, Raspberry Pi OS bullseye, and newer),
  plus paramiko for eftpgate's SFTP. No other packages on the Pi.
- **Security (guideline 7.3):** the home network only, or `--allow`; no
  connection back home without `--allow-local`, and only to the address
  that was checked; TLS 1.2 or newer with certificate and name checks.
  The log names who connected where, never paths, contents or passwords.
- **Nothing the Atari sees changes by accident:** the refusal and status
  texts, the login forms and the ports are part of each app's protocol.
- **An update keeps everything:** settings (read from the unit's
  ExecStart), `Environment=` lines, drop-ins, users,
  `/var/lib/eftpgate/known_hosts`, `/etc/claude-st` and
  `/var/lib/claude-st`. A new gateway replaces the old one only once it
  answers; otherwise the old one comes back.
- **Before pushing:** `python3 -m pyflakes gateway/*.py gateway/aera-gateway`
  and `python3 -m unittest discover -s gateway -p 'test_*.py'` (pip install
  paramiko pyftpdlib pyopenssl). CI runs `tests/systemd.sh` for real; run it
  yourself only on a throwaway computer.
- **Never commit:** real addresses from the owner's network (use
  `192.168.1.x`, `PI_IP`, `ATARI_IP`), passwords, session keys.
- British English, as in the app READMEs.
- **The name** reads "AERA | Erez Yaary" wherever it is given: copyright
  lines (`Copyright (C) 2026 AERA | Erez Yaary`), the docs, and as the
  author of every commit:
  `git commit --author="AERA | Erez Yaary <yaary@me.com>"`, with Claude as
  the signing committer (AERA-Atari, `standards/GITHUB-IDENTITY.md`).
