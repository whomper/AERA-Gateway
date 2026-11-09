# The Gateway in Docker

The Gateway's services can run in one Docker container instead of being
installed on a Raspberry Pi with `aera-gateway` (Erez, 9 Nov 2026): the
same programs, the same ports and the same rules, started by
`docker/aera-gateway-run` instead of systemd. The image comes with every
AERA-Gateway release from GitHub Packages:

| Image | What it is |
|---|---|
| `ghcr.io/whomper/aera-gateway:latest` | every service |
| `ghcr.io/whomper/aera-gateway:latest-browser` | the same, with the Gateway's browser for EWeb's snapshots, live view and Cloudflare's check (Chromium; about 400 MB more) |
| `ghcr.io/whomper/aera-gateway:1.40`, `:1.40-browser` | one release |

What each service does and speaks is in [SERVICES.md](SERVICES.md).

## Which systems

| System | Image | What works |
|---|---|---|
| **Raspberry Pi OS 64-bit** (bookworm, trixie) on a Raspberry Pi 4 or 5 | arm64 | everything; translate needs 4 GB of memory |
| **Debian 12 or newer, Ubuntu 22.04 or newer**, any 64-bit Linux with Docker 24 or newer (a PC, a NAS, a server) | amd64 or arm64 | everything |
| **macOS 13 or newer** (Apple silicon or Intel) with Docker Desktop | arm64 or amd64 | everything except two things: `AERA_ALLOW` can't single out your Atari, and ClaudeST is network only (no serial cable) |
| **Windows 10 or 11** with Docker Desktop (WSL 2) | amd64 | the same as macOS |

Not supported:
- **32-bit systems**, such as 32-bit Raspberry Pi OS: there is no 32-bit image (translate's engine has no 32-bit build). Install with `aera-gateway` there instead.
- **A computer that already runs the services**, installed with `aera-gateway`: both use the same ports. Use one or the other.

On Linux the container uses the host's network. The Ataris then reach each
service on the computer's own address, and each service sees the Atari's
own address, so `AERA_ALLOW` and the home-network rules work as on a
Raspberry Pi. Docker Desktop on macOS and Windows runs containers in a
virtual machine. It publishes the ports on the computer's address, but
every Atari arrives with Docker's own address. Leave `AERA_ALLOW` empty
there: the home network may use the Gateway, as without `--allow`.

The machine needs about 2 GB of memory for every service, and 4 GB with
translate. On disk it needs 1.5 GB for the image (2 GB with the browser),
and 300 MB to 1.1 GB for translate's languages.

## Starting it

On Linux, with Docker and its compose plugin installed (on Raspberry Pi
OS: `curl -fsSL https://get.docker.com | sh`):

```
mkdir ~/aera-gateway && cd ~/aera-gateway
curl -fsSLO https://raw.githubusercontent.com/whomper/AERA-Gateway/main/docker/compose.yml
nano compose.yml          # your Atari's address, your mail provider's servers
docker compose up -d
docker compose logs -f    # what the services say
```

On macOS or Windows, with Docker Desktop: the same with
`compose.desktop.yml`, and `docker compose -f compose.desktop.yml up -d`.

The settings are in the compose file's `environment:`:

| Setting | What it does |
|---|---|
| `AERA_ALLOW` | your Atari's address (several: `IP1,IP2`); empty: the home network (Linux only) |
| `AERA_MAIL` | EMail's servers: `"imap.example.com smtp.example.com[:587] [pop.example.com]"`; empty: no mail |
| `AERA_SERVICES` | `all`, or some of `mail dav ftp web pdf translate radio claude` |
| `AERA_LANGUAGES` | translate's languages, downloaded at the first start (`en,de`; then from the apps' Languages page) |
| `AERA_BROWSER` | `1` with the `-browser` image: the Gateway's browser for EWeb |
| `AERA_ALLOW_LOCAL` | `1`: dav, ftp, web and radio may reach servers and stations on the home network |
| `AERA_VERBOSE` | `1`: every service logs every request |
| `AERA_ARGS_PDF`, `AERA_ARGS_WEB`... | more of one service's own options, e.g. `AERA_ARGS_PDF: "--time 900"` |
| `CLAUDE_BACKEND` | ClaudeST's: `claudeai` (default), `api` or `demo` |
| `CLAUDE_SESSION_KEY`, `ANTHROPIC_API_KEY` | ClaudeST's key, taken at the first start (or set it later, below) |

In each Atari program, the Gateway is the computer's address, with the
ports in the [README](../README.md).

## Keeping it up to date

```
docker compose pull && docker compose up -d
```

The container is made again from the new image. What is kept is in the
`aera-gateway` volume (`/data` in the container), and a new container finds
it there:
- FTP's SSH host keys;
- translate's languages;
- ClaudeST's settings, key and chats;
- the browser's profile, with its logins.

A new claude.ai session key:

```
docker exec -it aera-gateway aera-gateway-run set-key
```

`docker exec aera-gateway aera-gateway-run status` says what runs.

## How it differs from a Raspberry Pi install

- systemd's jobs are `aera-gateway-run`'s:
  - it starts each service as its own user, and starts it again after 5 s if it stops;
  - mail alone may bind its ports below 1024;
  - it starts aerapens and aerajsc once a connection, with their memory and time limits;
  - it runs translate's Languages job when an app asks for one;
  - with the `-browser` image, it starts the Gateway's browser.
- Each service logs to the container's output (`docker logs`), not the journal.
- Chromium runs without its own sandbox, because the container is one. On a Raspberry Pi install it keeps its sandbox.
- There is no `aera-gateway` command in the container: the compose file holds the settings, and an image replaces `update-pi.sh`.

## Building it yourself

From the repository's root:

```
docker build -f docker/Dockerfile -t aera-gateway .
docker build -f docker/Dockerfile --build-arg BROWSER=1 -t aera-gateway:browser .
```

`tests/docker.sh` runs it for real: every service on its port, each as its
own user, what `/data` keeps, a service started again, no key in the log.
CI runs it on every push.
