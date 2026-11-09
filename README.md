# AERA-Gateway

<img src="docs/icon.png" width="96" align="right" alt="AERA-Gateway icon: an Atari ST, a Raspberry Pi and the cloud, connected">

The Gateway is the Raspberry Pi side of the AERA Atari programs: the
services that give an Atari (ST, STE, TT, Falcon; TOS or MagiC, STinG)
modern mail, calendars, contacts, file transfer, the web, PDFs,
translation, web radio and Claude, and one program, `aera-gateway`, that
sets them up, keeps them up to date and says how they are.

| Service     | For                 | Port(s) on the Gateway           | What it does                                       |
|-------------|---------------------|----------------------------------|----------------------------------------------------|
| `mail`      | EMail               | 143 IMAP, 587 SMTP, 110 POP3     | plain mail in, TLS to your mail provider           |
| `dav`       | ECal and EContact   | 8180                             | HTTPS to CalDAV, CardDAV and calendar feeds, feeds trimmed |
| `ftp`       | EFTP                | 2121, and 30000-30099 for data   | plain FTP in, FTPS or SFTP to the server           |
| `web`       | EWeb                | 8280                             | HTTP and HTTPS to any web site; pictures made smaller or as the Atari's pens, scripts compiled, pages translated, snapshots in the Gateway's browser |
| `pdf`       | EConvert and EPDF   | 8380                             | PDFs converted (and translated) on the Gateway     |
| `translate` | EMail, EWeb, EPDF   | 8480                             | text translated on the Gateway by a local engine   |
| `radio`     | EPlay               | 8580                             | web radio from MP3, AAC, Ogg or HLS stations, sent to the Atari as MP3, with titles, covers and logos |
| `claude`    | ClaudeST            | 2323 (or a serial cable)         | HTTPS to claude.ai or the Anthropic API            |

**[docs/SERVICES.md](docs/SERVICES.md)** is the reference, service by
service: how each is installed, how an Atari program talks to it, what it
answers and refuses (in the exact words the Atari gets), its limits and
what it logs.

In the instructions, `GATEWAY_IP` is the Gateway's (the Raspberry Pi's) address on your home network and
`ATARI_IP` the Atari's.

## Getting started

On a new Raspberry Pi (Raspberry Pi OS bullseye or newer, or any Debian or
Ubuntu with Python 3.9 or newer), one line sets up everything for your
Atari (`ATARI_IP`), with your mail provider's servers:

```
sudo apt install -y git && git clone https://github.com/whomper/AERA-Gateway ~/AERA-Gateway && ~/AERA-Gateway/update-pi.sh --allow ATARI_IP --mail imap.gmail.com smtp.gmail.com
```

It takes the copy to AERA-Gateway's latest release and installs every
service (`aera-gateway install all`): leave out `--mail` for no mail, add
`--browser` for EWeb's snapshots on a Raspberry Pi with 2 GB or more.
`translate` needs a 64-bit system and 4 GB, and is left out (with the
reason) on a smaller one. One that fails doesn't stop the others. Or one
at a time, only the ones you need:

```
sudo aera-gateway install mail imap.gmail.com smtp.gmail.com pop.gmail.com --allow ATARI_IP
sudo aera-gateway install dav --allow ATARI_IP
sudo aera-gateway install ftp --allow ATARI_IP
sudo aera-gateway install web --allow ATARI_IP
sudo aera-gateway install pdf --allow ATARI_IP
sudo aera-gateway install translate --allow ATARI_IP     (English and German; --languages en,de,he for more)
sudo aera-gateway install radio --allow ATARI_IP
sudo aera-gateway install claude --network --atari ATARI_IP
```

`install pdf` builds EPDF's converter (a few minutes), `install translate`
downloads the engine and its languages (about 290 MB), and `install radio`
adds ffmpeg.

(`update-pi.sh` makes `aera-gateway` a command; before it has run once,
it is `~/AERA-Gateway/gateway/aera-gateway`.) Each prints what to type
into its Atari program:

| Atari program      | Where                                  | What                                          |
|--------------------|----------------------------------------|-----------------------------------------------|
| EMail              | Options > Accounts                     | server `GATEWAY_IP`; IMAP 143 (or POP3 110), SMTP 587 |
| ECal, EContact     | Settings > Connection                  | Gateway `GATEWAY_IP`, port 8180                 |
| EFTP               | Options > Settings > Secure            | Gateway `GATEWAY_IP`, port 2121                 |
| EWeb               | Settings > Secure                      | Gateway `GATEWAY_IP`, port 8280                 |
| ClaudeST           | Options > Settings > Network           | `GATEWAY_IP`, port 2323                            |
| EConvert, EPDF     | Gateway mode: Through the Gateway      | `GATEWAY_IP`, port 8380                       |
| EMail, EWeb, EPDF  | Settings: Translate                    | Gateway `GATEWAY_IP`, port 8480               |
| EPlay              | Settings: Gateway                      | `GATEWAY_IP`, port 8580                       |

Use the Gateway's address, not its name: plain TOS with STinG can't always
look names up on the home network. On a Falcon or TT, EMail, ECal,
EContact, EFTP and EWeb can make the secure connections themselves
(their Falcon mode); the Gateway is still needed for SFTP, and for EWeb's
smaller pictures, pens, compiled scripts, translation and snapshots.

Then, now and then:

```
~/AERA-Gateway/update-pi.sh
```

It takes the copy to the latest release and runs
`sudo aera-gateway update`: every service installed brought up to date,
ClaudeST's bridge included, all settings, keys,
chats and languages kept. Without options it adds no service: a new one
is `install SERVICE`, or `update-pi.sh --allow ATARI_IP` for all of them.
The Gateway runs releases only, never work in progress.

`aera-gateway status` says what runs where, and
`sudo aera-gateway remove SERVICE` takes one away. Setting each one up
is in [gateway/README.md](gateway/README.md), with
[When something doesn't work](gateway/README.md#when-something-doesnt-work);
what each one speaks is in [docs/SERVICES.md](docs/SERVICES.md).

A Raspberry Pi set up with the apps' own `install.sh` (before the gateways moved
here) is taken over by `update`: the same services, ports and settings.

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
tests/translate/  the fixed Hebrew/English set translate-check.py uses (no private text)
update-pi.sh      this copy at the latest release, then aera-gateway update (with --allow: install all)
tests/systemd.sh  aera-gateway for real, on a computer with systemd
tests/snap.sh     install web --browser for real with Ubuntu's Chromium snap
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
```

CI runs all three on every push, the first with Python 3.9, 3.11, 3.12 and
3.13 (Raspberry Pi OS bullseye, bookworm and trixie, Ubuntu 24.04).

## Licence

Copyright (C) 2026 AERA | Erez Yaary. The gateways, the installer and the notes here are free software under
the GNU General Public License, version 2 or (at your option) any later
version; see [`COPYING`](COPYING). The decoders in `pens/common/third_party/`
(libjpeg-turbo, TJpgDec, libnsgif) and QuickJS in `jsc/eweb/third_party/`
(MIT) keep their own licences, which are beside them.

---

Part of **AERA** for Atari: [ECal](https://github.com/whomper/ECal) · [EContact](https://github.com/whomper/EContact) · [EMail](https://github.com/whomper/EMail) · [EFTP](https://github.com/whomper/EFTP) · [EHypView](https://github.com/whomper/EHypView) · [ClaudeST](https://github.com/whomper/ClaudeST) · [EWeb](https://github.com/whomper/EWeb) · [EPDF](https://github.com/whomper/EPDF) · [EPlay](https://github.com/whomper/EPlay) · [EUpdate](https://github.com/whomper/EUpdate) · 8-bit: [EDOS](https://github.com/whomper/EDOS) · [EFTP_XL](https://github.com/whomper/EFTP_XL) · Pi: [AERA-Gateway](https://github.com/whomper/AERA-Gateway)
