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

## Install

On a Raspberry Pi (Raspberry Pi OS bullseye or newer, or any Debian or
Ubuntu with Python 3.9 or newer), one line sets up every service for your
Atari, here with your mail provider's servers:

```
sudo apt install -y git && git clone https://github.com/whomper/AERA-Gateway ~/AERA-Gateway && ~/AERA-Gateway/update-pi.sh --allow ATARI_IP --mail imap.gmail.com smtp.gmail.com
```

Or one service at a time (`sudo aera-gateway install web --allow ATARI_IP`).
Then, now and then, `~/AERA-Gateway/update-pi.sh`: the latest release,
every setting kept. `aera-gateway status` says what runs where.

Or in Docker, on 64-bit Linux, or on macOS and Windows with Docker Desktop:

```
curl -fsSLO https://raw.githubusercontent.com/whomper/AERA-Gateway/main/docker/compose.yml
nano compose.yml && docker compose up -d
```

`GATEWAY_IP` is the Gateway's address on your home network, `ATARI_IP` the
Atari's; each Atari program takes `GATEWAY_IP` and its service's port.

## Documentation

- [gateway/README.md](gateway/README.md): setting up each service, the Atari programs' settings, when something doesn't work
- [docs/SERVICES.md](docs/SERVICES.md): what each service speaks: requests, answers, refusals, limits, logs
- [docs/DOCKER.md](docs/DOCKER.md): the Gateway in Docker, and on which systems
- [docs/DEVELOPING.md](docs/DEVELOPING.md): the source, the apps' copies, releases, tests

## Building and tests

Nothing to build by hand: `install pdf` and `install web` build aerapdf,
aerapens and aerajsc on the Gateway. The tests:

```
pip install paramiko pyftpdlib pyopenssl pillow
python3 -m unittest discover -s gateway -p 'test_*.py'
```

`tests/systemd.sh`, `tests/snap.sh` and `tests/docker.sh` install for real,
on a spare computer; CI runs them all on every push.

## Licence

Copyright (C) 2026 AERA | Erez Yaary. The gateways, the installer and the notes here are free software under
the GNU General Public License, version 2 or (at your option) any later
version; see [`COPYING`](COPYING). The decoders in `pens/common/third_party/`
(libjpeg-turbo, TJpgDec, libnsgif) and QuickJS in `jsc/eweb/third_party/`
(MIT) keep their own licences, which are beside them.

---

Part of **AERA** for Atari: [ECal](https://github.com/whomper/ECal) · [EContact](https://github.com/whomper/EContact) · [EMail](https://github.com/whomper/EMail) · [EFTP](https://github.com/whomper/EFTP) · [EHypView](https://github.com/whomper/EHypView) · [ClaudeST](https://github.com/whomper/ClaudeST) · [EWeb](https://github.com/whomper/EWeb) · [EPDF](https://github.com/whomper/EPDF) · [EPlay](https://github.com/whomper/EPlay) · [EUpdate](https://github.com/whomper/EUpdate) · 8-bit: [EDOS](https://github.com/whomper/EDOS) · [EFTP_XL](https://github.com/whomper/EFTP_XL) · Pi: [AERA-Gateway](https://github.com/whomper/AERA-Gateway)
