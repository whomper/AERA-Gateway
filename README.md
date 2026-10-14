# AERA-Gateway

Raspberry Pi gateway that gives an Atari Falcon (TOS/MagiC, STinG) access to modern email and the modern web.

| Folder | What it is |
|---|---|
| [`email-proxy/`](email-proxy/) | troll-proxy: IMAP (port 1143) for the TROLL mail client and iCloud Mail. Rewrites only what TROLL sees; originals stay untouched. |
| [`web-proxy/`](web-proxy/) | Notes and settings for the web side (WRP and Macproxy Classic on the same Pi). |

In the instructions, `PI_IP` is the Pi's address on your home network and `ATARI_IP` the Falcon's.

## Mail ports on the Pi

| Port | Used by | Proxy |
|---|---|---|
| 143 (IMAP), 587 (SMTP) | EMAIL.PRG | mail-proxy, from [whomper/EMail `gateway/`](https://github.com/whomper/EMail/tree/main/gateway) |
| 1143 (IMAP) | TROLL | troll-proxy, this repository's [`email-proxy/`](email-proxy/) |
| 25 (SMTP) | TROLL | Postfix on the Pi |

## Licence

The scripts and notes here are free software under the GNU General Public
License, version 2 or (at your option) any later version; see
[`COPYING`](COPYING). `web-proxy/wrp-custom.patch` changes
[WRP](https://github.com/tenox7/wrp) and stays under WRP's own licence.
