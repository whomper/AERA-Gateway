# AERA-Gateway

<img src="docs/icon.png" width="96" align="right" alt="AERA-Gateway icon: an Atari ST, a Raspberry Pi and the cloud, connected">

The Raspberry Pi side of the AERA Atari programs: the gateways that give
an Atari (ST, STE, TT, Falcon; TOS or MagiC, STinG) modern mail,
calendars, contacts, file transfer, the web and Claude, and one program,
`aera-gateway`, that sets them up, keeps them up to date and says how
they are.

| Service  | For                 | Port(s) on the Pi                    | What the Pi does onwards              |
|----------|---------------------|--------------------------------------|---------------------------------------|
| `mail`   | EMail               | 143 IMAP, 587 SMTP, 110 POP3         | TLS to your mail provider             |
| `dav`    | ECal and EContact   | 8180                                 | HTTPS to CalDAV, CardDAV and feeds    |
| `ftp`    | EFTP                | 2121, and 30000-30099 for data       | FTPS or SFTP to the server            |
| `web`    | EWeb                | 8280                                 | HTTP and HTTPS to any web server      |
| `claude` | ClaudeST            | 2323 (or a serial cable)             | HTTPS to claude.ai or the API         |

In the instructions, `PI_IP` is the Pi's address on your home network and
`ATARI_IP` the Atari's.

## Getting started

On a new Pi (Raspberry Pi OS bullseye or newer, or any Debian or Ubuntu
with Python 3.9 or newer), one line sets up everything for your Atari
(`ATARI_IP`), with your mail provider's servers:

```
sudo apt install -y git && git clone https://github.com/whomper/AERA-Gateway ~/AERA-Gateway && ~/AERA-Gateway/update-pi.sh --allow ATARI_IP --mail imap.gmail.com smtp.gmail.com
```

It takes the copy to AERA-Gateway's latest release and installs mail,
dav, ftp, web and claude (`aera-gateway install all`): leave out `--mail`
for no mail, add `--browser` for EWeb's snapshots on a Pi with 2 GB or
more. One that fails doesn't stop the others. Or one at a time, only the
ones you need:

```
sudo aera-gateway install mail imap.gmail.com smtp.gmail.com pop.gmail.com --allow ATARI_IP
sudo aera-gateway install dav --allow ATARI_IP
sudo aera-gateway install ftp --allow ATARI_IP
sudo aera-gateway install web --allow ATARI_IP
sudo aera-gateway install claude --network --atari ATARI_IP
```

(`update-pi.sh` makes `aera-gateway` a command; before it has run once,
it is `~/AERA-Gateway/gateway/aera-gateway`.) Each prints what to type
into its Atari program:

| Atari program      | Where                                  | What                                          |
|--------------------|----------------------------------------|-----------------------------------------------|
| EMail              | Options > Accounts                     | server `PI_IP`; IMAP 143 (or POP3 110), SMTP 587 |
| ECal, EContact     | Settings > Connection                  | Pi gateway `PI_IP`, port 8180                 |
| EFTP               | Options > Settings > Secure            | Pi gateway `PI_IP`, port 2121                 |
| EWeb               | Settings > Secure                      | Pi gateway `PI_IP`, port 8280                 |
| ClaudeST           | Options > Settings > Network           | `PI_IP`, port 2323                            |

Use the Pi's address, not its name: plain TOS with STinG can't always
look names up on the home network. On a Falcon or TT, EMail, ECal,
EContact, EFTP and EWeb can make the secure connections themselves
(their Falcon mode); the Pi is still needed for SFTP, and for EWeb's
smaller pictures, pens, compiled scripts and snapshots.

Then, now and then:

```
~/AERA-Gateway/update-pi.sh
```

It takes the copy to the latest release and runs
`sudo aera-gateway update`: every gateway on the Pi brought up to
date, ClaudeST's bridge from ClaudeST's latest release, all settings,
keys and chats kept. The Pi runs releases only, never work in progress.

`aera-gateway status` says what runs where, and
`sudo aera-gateway remove SERVICE` takes one away. The whole
story, service by service, is in [gateway/README.md](gateway/README.md),
and [When something doesn't work](gateway/README.md#when-something-doesnt-work)
is there too.

A Pi set up with the apps' own `install.sh` (before the gateways moved
here) is taken over by `update`: the same services, ports and settings.

## Source

```
gateway/      what runs on the Pi; the apps carry a copy of this folder
  aera-gateway    the installer: install (one service, or all), update, status, remove, set-key
  aeragate.py     the rules every gateway keeps: who may use it, where it may go, TLS, the log
  emailgate.py    mail: plain IMAP, SMTP and POP3 in, TLS out
  ecalgate.py     dav: plain HTTP in proxy form in, HTTPS out, feeds trimmed
  eftpgate.py     ftp: plain FTP in, FTPS or SFTP out
  webgate.py      web: plain HTTP in proxy form in, HTTP or HTTPS out
  webgate-ext/    webgate's extension for the Pi's browser (install web --browser)
  install.sh      the apps' old way in, handed on to aera-gateway
  test_*.py       their tests
pens/             aerapens, for webgate's pictures as the Atari's pens: built on the Pi by install web
  common/         its source, AERA-Atari's common/ at the commit in pens/COMMON (tools/sync-common.sh)
jsc/              aerajsc, for webgate's scripts compiled for EWeb: built on the Pi by install web
  eweb/           its source, EWeb's tools/jsc and QuickJS at the commit in jsc/EWEB (tools/sync-jsc.sh)
tools/
  sync-common.sh  pens/common/ from AERA-Atari; never edit pens/common/ here
  sync-jsc.sh     jsc/eweb/ from EWeb; never edit jsc/eweb/ here
update-pi.sh      this copy at the latest release, then aera-gateway update (with --allow: install all)
tests/systemd.sh  aera-gateway for real, on a computer with systemd
```

ClaudeST's bridge is ClaudeST's own (it changes with `CLAUDEST.PRG`):
`aera-gateway` sets it up from a ClaudeST release or copy.

## The apps' copies

EMail, ECal, EContact, EFTP, EWeb and ClaudeST carry `gateway/` as a
read-only copy (a git subtree of this repository's `gateway` branch,
brought up to date with their `tools/sync-gateway.sh`), so their tests
run against the gateway they were made with. Change the gateways here, never in a copy.
The `gateway` branch is `git subtree split --prefix=gateway` of `main`.

## Releases

The Pi gets formal releases only, vX.Y, made from `main` by the Release
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
```

CI runs both on every push, the first with Python 3.9, 3.11, 3.12 and 3.13
(Raspberry Pi OS bullseye, bookworm and trixie, Ubuntu 24.04).

## Licence

Copyright (C) 2026 AERA | Erez Yaary. The gateways, the installer and the notes here are free software under
the GNU General Public License, version 2 or (at your option) any later
version; see [`COPYING`](COPYING). The decoders in `pens/common/third_party/`
(libjpeg-turbo, TJpgDec, libnsgif) and QuickJS in `jsc/eweb/third_party/`
(MIT) keep their own licences, which are beside them.

---

Part of **AERA** for Atari: [ECal](https://github.com/whomper/ECal) · [EContact](https://github.com/whomper/EContact) · [EMail](https://github.com/whomper/EMail) · [EFTP](https://github.com/whomper/EFTP) · [EHypView](https://github.com/whomper/EHypView) · [ClaudeST](https://github.com/whomper/ClaudeST) · [EWeb](https://github.com/whomper/EWeb) · [EUpdate](https://github.com/whomper/EUpdate) · 8-bit: [EDOS](https://github.com/whomper/EDOS) · [EFTP_XL](https://github.com/whomper/EFTP_XL) · Pi: [AERA-Gateway](https://github.com/whomper/AERA-Gateway)
