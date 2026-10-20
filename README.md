# AERA-Gateway

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

On the Pi (Raspberry Pi OS bullseye or newer, or any Debian or Ubuntu
with Python 3.9 or newer), in your home folder:

```
sudo apt install -y git                   Raspberry Pi OS Lite comes without it
git clone https://github.com/whomper/AERA-Gateway
cd AERA-Gateway
./update-pi.sh
sudo gateway/aera-gateway install mail imap.gmail.com smtp.gmail.com pop.gmail.com
sudo gateway/aera-gateway install dav
sudo gateway/aera-gateway install ftp
sudo gateway/aera-gateway install web
sudo gateway/aera-gateway install claude --network --atari 192.168.1.20
```

`./update-pi.sh` takes this copy to AERA-Gateway's latest release. Install
only the ones you need, with your own mail provider's servers and your
Atari's address; each prints what to type into its Atari program:

| Atari program      | Where                                  | What                                          |
|--------------------|----------------------------------------|-----------------------------------------------|
| EMail              | Options > Accounts                     | server `PI_IP`; IMAP 143 (or POP3 110), SMTP 587 |
| ECal, EContact     | Settings > Connection                  | Pi gateway `PI_IP`, port 8180                 |
| EFTP               | Options > Settings > Secure            | Pi gateway `PI_IP`, port 2121                 |
| EWeb               | `GATEWAY=` in `EWEB.INF`               | `PI_IP:8280`                                  |
| ClaudeST           | Options > Settings > Network           | `PI_IP`, port 2323                            |

Use the Pi's address, not its name: plain TOS with STinG can't always
look names up on the home network. On a Falcon or TT, EMail, ECal,
EContact and EFTP can make the secure connections themselves (their
Falcon mode) and need the Pi only for SFTP.

Then, now and then, the same:

```
./update-pi.sh
```

It takes this copy to the latest release and runs
`sudo gateway/aera-gateway update`: every gateway on the Pi brought up to
date, ClaudeST's bridge from ClaudeST's latest release, all settings,
keys and chats kept. The Pi runs releases only, never work in progress.

`gateway/aera-gateway status` says what runs where, and
`sudo gateway/aera-gateway remove SERVICE` takes one away. The whole
story, service by service, is in [gateway/README.md](gateway/README.md),
and [When something doesn't work](gateway/README.md#when-something-doesnt-work)
is there too.

A Pi set up with the apps' own `install.sh` (before the gateways moved
here) is taken over by `update`: the same services, ports and settings.

## TROLL's proxy is gone

AERA-Gateway no longer has troll-proxy (IMAP for the TROLL mail client on
port 1143). A Pi that still runs it can let it go:

```
sudo systemctl disable --now falcon-imap-logproxy
sudo rm /etc/systemd/system/falcon-imap-logproxy.service
sudo rm -r /opt/falcon-imap
sudo systemctl daemon-reload
```

and, if ufw let the Atari in on 1143, `sudo ufw status numbered` and
`sudo ufw delete N` for that rule. Postfix on port 25 was there for TROLL
too; remove it if nothing else uses it.

## The web

EWeb goes through `web` (webgate). The web proxies for CAB and Highwire
(WRP and Macproxy Classic, in Docker) are set up by hand: [web-proxy/README.md](web-proxy/README.md)
has the steps, WRP's patch and the firewall rules that keep them to the
Atari.

## Source

```
gateway/      what runs on the Pi; the apps carry a copy of this folder
  aera-gateway    the installer: install, update, status, remove, set-key
  aeragate.py     the rules every gateway keeps: who may use it, where it may go, TLS, the log
  emailgate.py    mail: plain IMAP, SMTP and POP3 in, TLS out
  ecalgate.py     dav: plain HTTP in proxy form in, HTTPS out, feeds trimmed
  eftpgate.py     ftp: plain FTP in, FTPS or SFTP out
  webgate.py      web: plain HTTP in proxy form in, HTTP or HTTPS out
  webgate-ext/    webgate's browser extension, for its snapshots (not set up yet)
  install.sh      the apps' old way in, handed on to aera-gateway
  test_*.py       their tests
update-pi.sh      the Pi brought up to date: this copy at the latest release, then aera-gateway update
tests/systemd.sh  aera-gateway for real, on a computer with systemd
web-proxy/    notes and WRP's patch for the web proxies
```

ClaudeST's bridge is ClaudeST's own (it changes with `CLAUDEST.PRG`):
`aera-gateway` sets it up from a ClaudeST release or copy.

## The apps' copies

EMail, ECal, EContact, EFTP, EWeb and ClaudeST carry `gateway/` as a read-only
copy (a git subtree of this repository's `gateway` branch, brought up to
date with their `tools/sync-gateway.sh`), so their tests run against the
gateway they were made with. Change the gateways here, never in a copy.
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
pip install paramiko pyftpdlib pyopenssl
python3 -m unittest discover -s gateway -p 'test_*.py'
sudo tests/systemd.sh          on a spare computer with systemd: it installs and removes services
```

CI runs both on every push, the first with Python 3.9, 3.11, 3.12 and 3.13
(Raspberry Pi OS bullseye, bookworm and trixie, Ubuntu 24.04).

## Licence

The gateways, the installer and the notes here are free software under
the GNU General Public License, version 2 or (at your option) any later
version; see [`COPYING`](COPYING). `web-proxy/wrp-custom.patch` changes
[WRP](https://github.com/tenox7/wrp) and stays under WRP's own licence.

---

Part of the AERA Atari family: [ECal](https://github.com/whomper/ECal),
[EContact](https://github.com/whomper/EContact),
[EMail](https://github.com/whomper/EMail),
[EFTP](https://github.com/whomper/EFTP),
[EWeb](https://github.com/whomper/EWeb),
[EHypView](https://github.com/whomper/EHypView),
[ClaudeST](https://github.com/whomper/ClaudeST) and
[AERA-Atari](https://github.com/whomper/AERA-Atari).
