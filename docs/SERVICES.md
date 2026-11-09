# The Gateway's services

This is the reference for every service the Gateway runs: what each is for,
how it is installed, how an Atari program talks to it, what it answers, what
it refuses (with the exact words the Atari gets), its limits, and what it
logs. It is written from the code as of AERA-Gateway 1.36. For setting the
Gateway up, see the [README](../README.md) and
[gateway/README.md](../gateway/README.md).

The protocols are the apps': each app's session decides its service's
requests and words, and AERA-Gateway carries them out (CLAUDE.md, "Who
decides what"). Where an app keeps the full description itself, the section
links to it.

In the examples, `GATEWAY_IP` is the Gateway's address on your home network,
`ATARI_IP` the Atari's, and `example.org` stands for any server.

## Contents

- [The rules every service keeps](#the-rules-every-service-keeps)
- [The installer](#the-installer)
- [mail: EMail](#mail-email)
- [dav: ECal and EContact](#dav-ecal-and-econtact)
- [ftp: EFTP](#ftp-eftp)
- [web: EWeb](#web-eweb)
- [pdf: EConvert and EPDF](#pdf-econvert-and-epdf)
- [translate: translation for the apps](#translate-translation-for-the-apps)
- [radio: EPlay](#radio-eplay)
- [claude: ClaudeST](#claude-claudest)

| Service     | For                       | Port(s)                          | Onwards                                         |
|-------------|---------------------------|----------------------------------|-------------------------------------------------|
| `mail`      | EMail                     | 143 IMAP, 587 SMTP, 110 POP3     | TLS to the mail provider                        |
| `dav`       | ECal, EContact            | 8180                             | HTTPS to CalDAV, CardDAV and calendar feeds     |
| `ftp`       | EFTP                      | 2121; 30000-30099 for data       | FTPS or SFTP to the server                      |
| `web`       | EWeb                      | 8280 (8281 on 127.0.0.1 only)    | HTTP and HTTPS to any web site                  |
| `pdf`       | EConvert, EPDF            | 8380                             | nothing: converts on the Gateway                |
| `translate` | EMail, EWeb, EPDF         | 8480; a local socket             | nothing: a local engine translates              |
| `radio`     | EPlay                     | 8580                             | HTTP and HTTPS to radio stations; radio-browser.info for logos |
| `claude`    | ClaudeST                  | 2323, or a serial cable          | HTTPS to claude.ai or the Anthropic API         |

A port never moves without the owning app's OK: the Ataris have them in
their settings.

## The rules every service keeps

These come from `gateway/aeragate.py` (guideline 7.3), and every section
below says where a service differs.

- **Who may connect.** Without `--allow`, any address on the home network:
  10.0.0.0/8, 172.16.0.0/12, 192.168.0.0/16, the Gateway itself (127.0.0.0/8),
  169.254.0.0/16, and IPv6 fc00::/7 and fe80::/10. With `--allow ATARI_IP`,
  only the addresses named.
- **Where it may connect onwards.** Not back into the home network (nor to
  the Gateway itself) unless the service is installed with `--allow-local`;
  and only to the address that was checked, so a second name look-up can't
  lead home. (`mail` connects only to the servers named at install, and the
  check doesn't apply to it.)
- **TLS.** TLS 1.2 or newer, with the server's certificate and name checked
  against the system's root certificates (or `--cafile`, where a service has
  it).
- **The log** (the systemd journal: `journalctl -u NAME`) says who connected
  where and how much, never paths, contents, file names or passwords.
- **Standard Python 3 only** (3.9 or newer), plus what a section names under
  *Needs*.

Each service runs as a systemd unit, as its own system user, with
`NoNewPrivileges`, `ProtectSystem=strict`, `ProtectHome` and `PrivateTmp`,
from `/opt/aera-gateway/`.

## The installer

```
sudo aera-gateway install SERVICE [options]     one service (again: change some of its settings)
sudo aera-gateway install all --allow ATARI_IP [--mail IMAP SMTP[:587] [POP3]] [--browser]
sudo aera-gateway update                        every installed service, from this copy
sudo aera-gateway remove SERVICE
aera-gateway status                             what runs, on which ports, with which settings
sudo aera-gateway set-key                       a new claude.ai session key for ClaudeST
~/AERA-Gateway/update-pi.sh [--allow ATARI_IP ...]    the latest release, then update (with options: install all)
```

- Every service takes `--allow ATARI_IP` (repeat it, or `IP1,IP2`; `any`
  goes back to the whole home network). `install SERVICE --help` lists a
  service's own options.
- Running `install` again changes only the options you name.
- **An update keeps** the settings (read back from the unit's `ExecStart`),
  the unit's `Environment=` and `EnvironmentFile=` lines, drop-ins and the
  service's user and state. A new version replaces the old one only once it
  answers on its ports; otherwise the old one comes back.
- If ufw is active, the installer opens each service's ports for the
  `--allow` addresses, or for the home networks.
- `install all` installs every service; it skips `mail` without `--mail`
  (unless installed already), `pdf` in an app's copy of `gateway/`, and
  `translate` on a 32-bit system or a machine with less than 4 GB, saying
  why. One that fails doesn't stop the others.
- Service names: `mail` (also `email`), `dav` (`cal`, `ecal`, `econtact`,
  `contacts`), `ftp` (`eftp`), `web` (`eweb`), `pdf` (`epdf`, `econvert`),
  `translate` (`translation`), `radio` (`eplay`), `claude` (`claudest`);
  each program's own name works too (`emailgate`, `webgate`...).

---

## mail: EMail

Plain IMAP, SMTP and POP3 from the Atari, passed on to the mail provider over
TLS, so that an Atari can use a provider that takes encrypted connections only.

- **Program:** `gateway/emailgate.py`. EMail's session owns the protocol
  (the STARTTLS handling and the "emailgate:" lines).
- **Ports:** 143 IMAP, 587 SMTP, 110 POP3 (POP3 only when a POP3 server is
  given). Onwards: IMAP 993, SMTP 465, POP3 995, all TLS; or `SERVER:587` for
  SMTP with STARTTLS. Any server may be given as `HOST:PORT`.

### Install

```
sudo aera-gateway install mail IMAP_SERVER SMTP_SERVER[:587] [POP3_SERVER] [--allow ATARI_IP]
```

| Option | What it does |
|---|---|
| `IMAP SMTP [POP3]` | the provider's servers; needed the first time |
| `--ports IMAP,SMTP,POP3` | other ports on the Gateway (two or three numbers) |
| `--allow ATARI_IP` | only this Atari |

The unit always logs connections (`-v`). It takes over EMail 1.x's stunnel
set-up (`/etc/stunnel/atari-mail.conf`), servers and ports included.

Command line: `emailgate.py [--ports 143,587,110] [--bind 0.0.0.0] [--allow IP] [-v] IMAP SMTP[:587] [POP3]`.

### How the Atari talks to it

EMail speaks plain IMAP, SMTP and POP3 to `GATEWAY_IP:143/587/110` as it would
to the provider, logins included (IMAP LOGIN, POP3 USER/PASS, SMTP AUTH,
with the provider's own user name and app password). The bytes pass both
ways unchanged.

With an SMTP server given as `:587`, the Gateway reads the server's
greeting, says `EHLO emailgate` and `STARTTLS` itself, starts TLS, and then
hands the Atari the server's greeting as if it had come straight from the
server; everything after that goes over TLS.

### Refusals

One line in the protocol's own form: IMAP `* BYE emailgate: WHY`, POP3
`-ERR emailgate: WHY`, SMTP `421 emailgate: WHY`.

| WHY | When |
|---|---|
| `this address may not use the gateway` | the Atari isn't allowed |
| `HOST's certificate doesn't check out (REASON)` | the provider's certificate or name check fails |
| `can't reach HOST:PORT (REASON)` | anything else on the way: not found, refused, a server that isn't SMTP, no STARTTLS offered or refused |

Once the relay runs, a failure closes the connection without words.

### Limits, log, needs

- 60 s to connect and for each send; a connection silent both ways for
  30 minutes is closed.
- **Log:** each connection, `PROTO ATARI_IP:PORT -> HOST:PORT, TLSv1.3`, and
  how it ended; never what was said, nor the passwords.
- **Needs:** standard Python 3 and ca-certificates.
- **State:** none.

---

## dav: ECal and EContact

The Atari's HTTP requests in proxy form, made onwards over HTTPS, with
calendar feeds trimmed to what the Atari shows. For CalDAV, CardDAV and
`.ics` subscriptions. Falcon mode doesn't use it.

- **Program:** `gateway/ecalgate.py`. ECal's session owns the protocol (the
  feed trimming and the texts).
- **Port:** 8180.

### Install

```
sudo aera-gateway install dav [--allow ATARI_IP]
```

| Option | What it does |
|---|---|
| `--port N` | another port (8180 by default) |
| `--allow-local` / `--no-allow-local` | also reach servers on the home network (a Radicale or Nextcloud at home) |
| `-v` / `--no-verbose` | log every request (the server, never the path) |

Command line: `ecalgate.py [--port 8180] [--bind 0.0.0.0] [--allow IP] [--allow-local] [-v]`.

### How the Atari talks to it

Plain HTTP/1.1 with the whole address on the request line; any method
(GET, PUT, DELETE, PROPFIND, REPORT, MKCALENDAR...):

```
PROPFIND https://caldav.example.org/calendars/ HTTP/1.1
Host: caldav.example.org
Authorization: Basic ...
Depth: 1
Content-Length: 211
```

- `https://` and `http://` addresses are taken; the body by its
  `Content-Length`. `Authorization` and the other headers pass on, less the
  hop-by-hop ones; `Host` is set from the address.
- **Feeds trimmed:** with `X-Calendar-Window: YYYYMMDD-YYYYMMDD` on a request
  whose answer is a calendar (200, a Content-Type with "calendar" or a body
  starting `BEGIN:VCALENDAR`), only the events in the window (two days wider
  each side) come back, with every repeating event that still repeats, the
  overrides of those, and the time zones; to-dos, journals, free/busy and
  bulky fields (HTML descriptions, attached files, Apple's structured
  locations) are left out. A CalDAV 207 is never trimmed; a badly formed
  window leaves the feed as it is.
- `GET http://GATEWAY_IP:8180/` from a browser answers
  `ecalgate 1.1 is running. Point ECal's gateway setting here.`

### Answers

The server's status and headers (less hop-by-hop ones, `Content-Encoding`
and `Set-Cookie`), with the body always unpacked and in one block with a
`Content-Length`. One HTTPS connection to each server is kept open for each
Atari connection.

### Refusals

| Status | Text | When |
|---|---|---|
| 403 | `ecalgate: this address may not use the gateway` | the Atari isn't allowed |
| 400 | `ecalgate: expected a request like GET https://host/path` | not an http or https address with a host |
| 403 | `ecalgate: won't connect to the local network (HOST)` | the server is on the home network, without `--allow-local` |
| 502 | `ecalgate: HOST has an untrusted certificate (REASON)` | the certificate or name check fails |
| 502 | `ecalgate: can't find HOST` | the name isn't found |
| 502 | `ecalgate: HOST closed the connection` | the server closed it twice running |
| 504 | `ecalgate: can't reach HOST (REASON)` | the server took over 60 s |
| 502 | `ecalgate: can't reach HOST (REASON)` | any other failure on the way |
| 502 | `ecalgate: HOST sent a broken compressed answer` | gzip or deflate that won't unpack |

### Limits, log, needs

- 60 s for each step with the server; answers are held whole in memory.
- **Log** (`-v`): `ATARI_IP METHOD https://host:443 -> STATUS, N bytes`, and
  `(trimmed from M)` for a feed. Never the path: a private feed's address is
  its password.
- **Needs:** standard Python 3 and ca-certificates. **State:** none.

---

## ftp: EFTP

Plain FTP from the Atari; FTPS or SFTP from the Gateway to the real server.
To the Atari it is an ordinary FTP server.

- **Program:** `gateway/eftpgate.py`. EFTP's session owns the protocol (the
  FTP server, FTPS session reuse, SFTP and known_hosts).
- **Ports:** 2121 for control; 30000-30099 for passive data. Falcon mode
  doesn't need it for FTPS.

### Install

```
sudo aera-gateway install ftp [--allow ATARI_IP]
```

| Option | What it does |
|---|---|
| `--port N` | another control port (2121 by default) |
| `--allow-local` / `--no-allow-local` | also reach servers on the home network (a NAS) |
| `--cafile CA.PEM` / `--no-cafile` | check FTPS servers against this CA instead of the system's roots |
| `-v` / `--no-verbose` | log every command (never the passwords) |

It installs `python3-paramiko` for SFTP if it is missing. An update and a
remove keep `/var/lib/eftpgate/known_hosts`.

Command line: `eftpgate.py [--port 2121] [--bind 0.0.0.0] [--allow IP] [--allow-local] [--state DIR] [--cafile PEM] [--pasv-ports LO-HI] [-v]`.

### How the Atari talks to it

The Atari names the server in its login, the way FTP proxies always have:

```
USER sftp:erez@my.server.example:22        (sftp, port 22 by default)
USER ftps:erez@ftp.example.org             (explicit FTPS, AUTH TLS, port 21)
USER ftps-implicit:erez@ftp.example.org    (implicit FTPS, port 990)
PASS the server's own password
```

- `ftpes` is another name for `ftps`. The user name runs to the last `@`, so it
  may contain one; an IPv6 host goes in `[...]`.
- Before login: `USER`, `PASS`, `QUIT`, `FEAT`, `SYST`, `NOOP`, `OPTS`.
- After login: `PWD`, `CWD`, `CDUP`, `MKD`, `RMD`, `DELE`, `RNFR`/`RNTO`,
  `SITE CHMOD`, `SIZE`, `MDTM`, `MFMT`, `MLST`, `MLSD`, `LIST`, `NLST`, `RETR`,
  `STOR`, `APPE`, `REST` (resume, both ways), `ABOR`, `TYPE A|I`, `MODE S`,
  `STRU F`, `PASV`, `EPSV`.
- Passive mode only, on the Gateway's ports 30000-30099. Listings are
  `MLSD` with exact sizes and UTC times; names are UTF-8.
- `TYPE A` to an SFTP server: the Gateway converts the line ends both ways.

### Answers

FTP's own replies, for example `220 eftpgate 1.1: log in as sftp:user@host,
ftps:user@host or ftps-implicit:user@host`, `230 logged in to HOST: SFTP to
HOST, CIPHER`, `213 SIZE`, `226 sent`.

### Refusals

| Code | Text | When |
|---|---|---|
| 421 | `eftpgate answers only the home network` | the Atari isn't allowed |
| 530 | `log in first` | a command before login |
| 530 | `log in as sftp:user@host[:port], ftps:user@host or ftps-implicit:user@host` | a USER that doesn't read |
| 503 | `USER first` | PASS without USER |
| 530 | `can't find HOST` | the name isn't found |
| 530 | `HOST is on the home network: start eftpgate with --allow-local to reach it` | without `--allow-local` |
| 530 | `SFTP needs paramiko on the Gateway: sudo apt install python3-paramiko` | no paramiko |
| 530 | `can't reach HOST:PORT: REASON` | SFTP can't connect |
| 530 | `HOST:PORT doesn't speak SSH: REASON` | the SSH handshake fails |
| 530 | `HOST's SSH key has changed: if that is right, remove it from /var/lib/eftpgate/known_hosts on the Gateway` | a remembered key differs |
| 530 | `HOST wants another kind of login (TYPES)` | no password login offered |
| 530 | `login incorrect` | the password is refused |
| 530 | `HOST's certificate didn't check out: REASON` | the FTPS certificate check fails |
| 530 | `can't connect to HOST:PORT: REASON` | FTPS can't connect |
| 502 | `CMD isn't something eftpgate does` | an unknown command |
| 502 | `passive mode only (PASV, EPSV)` | PORT or EPRT |
| 504 | `no TLS here: eftpgate does it for you, towards the server` | AUTH from the Atari |
| 504 | `TYPE A or I` / `stream mode` / `file structure` / `SITE CHMOD only` | another TYPE, MODE, STRU or SITE |
| 550 | `PATH: no such folder` / `PATH is a folder` / `no size for PATH` / `PATH: not there` / `no such file or folder` / `permission denied` | as they say |
| 503 | `RNFR first` | RNTO without RNFR |
| 501 | `REST needs a number` | |
| 425 | `no free port for the data connection` / `PASV or EPSV first` / `the data connection didn't come` / `the data connection came from somewhere else` | data connection problems |
| 426 | `the transfer stopped` | the data connection failed mid-way |
| 451 | the server's own text, or `the server went wrong: REASON` | the server failed |

### Limits, security, log, needs

- Command lines up to 4096 bytes; an idle control connection closes after
  15 minutes; SFTP connects in 20 s (keepalive every 60 s), FTPS in 30 s; a
  data connection must come within 30 s.
- **SSH host keys:** remembered the first time in
  `/var/lib/eftpgate/known_hosts` (`[host]:port` for another port) and checked
  every time after. **FTPS** data connections reuse the control connection's
  TLS session and always use `PROT P`.
- **Log:** logins (`logged in to HOST (SFTP to HOST, CIPHER)`) and logouts;
  with `-v`, every command and reply, `PASS` shown as `****`. Never passwords
  or file contents.
- **Needs:** standard Python 3; paramiko for SFTP.
- **State:** `/var/lib/eftpgate/known_hosts`.

---

## web: EWeb

EWeb's HTTP and HTTPS gateway. It makes the Atari's requests over TLS 1.2 or
1.3, and on request makes pictures smaller or turns them into the Atari's
pens, compiles scripts, translates pages, and opens pages in the Gateway's
own browser.

- **Program:** `gateway/webgate.py`, its browser extension `gateway/webgate-ext/`,
  aerapens (built from `pens/`: AERA-Atari's `common/`) and aerajsc (built from
  `jsc/`: EWeb's QuickJS). EWeb's session owns all of their protocols.
- **Ports:** 8280 for the Atari; 8281 on 127.0.0.1 only, for the extension
  (with `--browser`).

### Install

```
sudo aera-gateway install web [--allow ATARI_IP] [--browser]
```

| Option | What it does |
|---|---|
| `--port N` | another port (8280 by default) |
| `--allow-local` / `--no-allow-local` | also reach servers on the home network (a router's or a NAS's pages) |
| `--cafile CA.PEM` / `--no-cafile` | check servers against this CA instead of the system's roots |
| `-v` / `--no-verbose` | log every request |
| `--browser` / `--no-browser` | the Gateway's browser, for snapshots, live view and Cloudflare's check: Chromium, Xvfb and xdotool, on a machine with 2 GB or more |

It installs Pillow (`python3-pil`), and gcc and make to build aerapens and
aerajsc; each runs on a socket of its own (`/run/webgate-pens.sock`,
`/run/webgate-js.sock`), started by systemd for each request with little
memory and time. Without them webgate still works: pictures come as JPEG or
PNG and scripts as they came. An update keeps the browser's profile (your
logins there).

Command line: `webgate.py [--port 8280] [--bind 0.0.0.0] [--allow IP] [--allow-local] [--cafile F] [--browser] [--browser-port 8281] [--pens-socket P] [--js-socket P] [-v]`.

### How the Atari talks to it

Every request goes to `GATEWAY_IP:8280` in plain HTTP/1.1, in proxy form
(GET, HEAD and POST). The host name `webgate` is webgate's own:
`http://webgate/...` addresses are answered by webgate itself.

#### Pages and everything else

```
GET https://example.org/path?q=1 HTTP/1.1
Accept-Encoding: gzip
```

- The request goes on with the Atari's headers (cookies, Referer, User-Agent
  as they are), less the hop-by-hop ones; `Host` from the address. Only the
  compression the Atari asked for (gzip, deflate) is asked of the server.
- The answer comes back as it came, a piece at a time: status, headers,
  cookies, redirects and error pages (EWeb follows redirects and keeps cookies
  itself), the server's compression as it was. A server's own `X-Gateway-`
  headers never pass: only webgate says those.
- **`X-Gateway-TLS`** on each answer from an https server says how the Gateway
  reached it: `TLS 1.3, TLS_AES_128_GCM_SHA256`.
- `GET / HTTP/1.1` (not proxy form) answers
  `webgate 1.0 is running. Put this address in EWeb's gateway setting.`

#### Pictures

```
GET https://example.org/photo.webp HTTP/1.1
X-Gateway-Picture: fit=608
```

- `fit=WIDTH` (16-4096): a picture wider than that, or of a kind the Atari
  can't read (WebP and the like), comes as a JPEG (quality 80) or, when it
  has transparency or few colours, a 256-colour PNG, no wider than WIDTH and
  turned upright. A JPEG, PNG or GIF that already fits comes as it was.
- `deep=1` (with fit): a true-colour Atari; no colour reduction, JPEG quality 90.
- **Pens:** `fit=608;size=400x300;pens=16;pal=1a2b3c4d[;ed=lj|tj]` gives the
  picture as the Atari's own pens, made by aerapens
  (`Content-Type: application/x-aera-pens`). The colours go first, once:
  `POST http://webgate/palette` with 2-256 colours as R G B bytes answers 204
  with `X-Gateway-Palette: CRC`; a picture asked with colours webgate hasn't
  got answers 409 `webgate: send the colours first` with
  `X-Gateway-Palette: need`.
- The answer says what was done: `X-Gateway-Picture: WEBP 1200x800 to JPEG 608x405`
  (and `X-Gateway-Pens: aerapens COMMON BUILD` for pens). Its other headers
  (ETag, Cache-Control) are the server's.

#### Pictures made ahead

```
POST http://webgate/prefetch HTTP/1.1

Referer: https://example.org/page
https://example.org/a.jpg<TAB>fit=608;pens=16;pal=1a2b3c4d<TAB>sid=x
https://example.org/b.png<TAB>fit=608
```

A page's pictures, one a line (`URL<TAB>X-Gateway-Picture<TAB>Cookie`, up to
100, after optional `Referer:` and `Accept:` lines). webgate makes four at a
time and answers 204 with `X-Gateway-Prefetch: N` (409 if a line names
unknown colours). The Atari then asks for each as usual (the same address and
X-Gateway-Picture, without If-None-Match or If-Modified-Since) and gets the
one made, once. What isn't asked for in 3 minutes goes; 48 MB at most are
kept; the Atari's next list drops what of its last hasn't begun.

#### Scripts compiled

```
GET http://webgate/scripts HTTP/1.1        ->  200 "aerajsc TAG" (or 404 "webgate: no aerajsc")
GET https://example.org/app.js HTTP/1.1
X-Gateway-Script: TAG
```

A script of 4 MB or less, whose tag matches aerajsc's, comes compiled to
EWeb's QuickJS bytecode: `Content-Type: application/x-eweb-qjs` with
`X-Gateway-Script: TAG` (the server's ETag and dates kept). Otherwise, or if
it doesn't compile in 8 s, it comes as it came.

#### Pages translated

```
GET http://webgate/ HTTP/1.1               ->  200 "webgate 1.0", X-Gateway-Translate: 1
GET https://example.org/ HTTP/1.1
X-Gateway-Translate: to=en
```

EWeb sends the header only to a webgate that says `X-Gateway-Translate: 1`.
A 200 answer that is text/html, application/xhtml+xml or text/plain comes
translated into CODE by translategate on the Gateway (its local socket: the
text goes nowhere else). The header goes no further than webgate; the
server is asked for the page whole and unpacked.

- **Translated:** the text shown, `<title>`, and the `alt`, `placeholder`,
  `title` and `aria-label` attributes, and the label (`value`) of submit,
  button and reset inputs. Each piece of text between tags is translated on
  its own, so that links and markup stay.
- **Kept as they are:** tags, links, forms, URLs, numbers, comments; script,
  style, textarea, pre, code, kbd, samp, var, svg, math and template; and any
  element marked `translate="no"`, with all that is inside it.
- `<html lang="CODE" dir="rtl|ltr">` is set (rtl for he, ar, fa, ur, yi, ps
  and dv), and a `<meta charset>` becomes utf-8.
- **Answer:** `Content-Type: text/html; charset=utf-8`,
  `X-Translate-From: LANG` (what the page was in), `Cache-Control: no-store`;
  the server's ETag, Last-Modified, Expires and Content-Language go.
- **Already in CODE:** the page as it came, with `X-Translate-From` and
  `X-Translate-Same: 1`.
- **At most the first 64 KB of text, in 90 s:** the rest as it was, and
  `X-Translate-Partial: 64K`, or the KB done (`12K`) when the time ran out
  first.
- **Refused:** the page as it came, with `X-Translate-Refused: STATUS why`:
  translategate's own (`422 translategate: that pair of languages isn't installed`, a 503 or 504),
  or webgate's: `503 webgate: the translation service isn't running`,
  `504 webgate: the translation took over 90 s`, `413 webgate: the page is over 4 MB`,
  `400 webgate: X-Gateway-Translate: VALUE?`.
- Pages through the browser (snapshots, visits) aren't translated.

#### Snapshots (`--browser`)

```
GET https://example.org/ HTTP/1.1
X-Gateway-Snapshot: 1
```

The page is opened in Chromium on the Gateway (its own profile: logins made
there stay there). Once it has settled after its scripts ran, the extension
sends it back as plain HTML: no scripts, each canvas as a picture, the
fields as filled (never passwords), and a `<base>` so that EWeb fetches its
stylesheets and pictures through webgate as usual. The answer is always 200
`text/html; charset=utf-8`, with `X-Gateway-Snapshot-URL: WHERE_IT_ENDED_UP`.
One page at a time, 60 s at most.

#### Cloudflare's check (`--browser`)

A page (a GET whose Accept names text/html) answered 403 or 503 with
Cloudflare's check ("Just a moment...") is opened in the Gateway's browser,
which passes the check by itself, and comes to EWeb as a snapshot. The site
then stays on the browser for the visit: its pages go there straight away
until none has been asked for in 25 minutes. Nothing of the browser's pass
(cookie, User-Agent) leaves it. A check that wants a click comes as it was,
with `X-Gateway-Check: click`; one the browser couldn't get through, with
`X-Gateway-Check: failed`.

#### Live view (`--browser`)

`http://webgate/live?w=W&h=H[&url=ADDRESS]` is a page with a picture of the
browser's screen as a picture button, a field to type into and buttons for
keys: for what a snapshot can't do (a CAPTCHA, a login made by scripts).

| Address | What it does |
|---|---|
| `/live?w=W&h=H&url=ADDRESS` | the page; `w`/`h` EWeb's page size (the view is then 1:1, three screens tall); `url` opened first (`https://` added if it has none) |
| `/live/shot.jpg` | the screen, as a JPEG, or as pens when X-Gateway-Picture asks for them |
| `/live/click?at.x=X&at.y=Y` | a real click on the screen |
| `/live/type?t=TEXT[&enter=1]` | real typing, then Return |
| `/live/key?k=enter\|tab\|backspace\|esc\|up\|down\|back\|reload` | a key |
| `/live/page` | "As a page": a snapshot of what is shown |

Click, type and key answer 303 back to `/live`.

### Refusals

| Status | Text | When |
|---|---|---|
| 403 | `webgate: this address may not use the gateway` | the Atari isn't allowed |
| 400 | `webgate: expected a request like GET https://host/path` | not an http or https address with a host |
| 403 | `webgate: won't connect to the local network (HOST)` | the server is on the home network, without `--allow-local` |
| 502 | `webgate: HOST has an untrusted certificate (REASON)` | the certificate or name check fails |
| 502 | `webgate: can't find HOST` | the name isn't found |
| 502 | `webgate: HOST closed the connection` | the server went away |
| 504 | `webgate: can't reach HOST (REASON)` | the server took over 60 s |
| 502 | `webgate: can't reach HOST (REASON)` | any other failure on the way |
| 502 | `webgate: the picture from HOST is too big` | a picture over 40 MB |
| 409 | `webgate: send the colours first` (and `X-Gateway-Palette: need`) | pens with unknown colours |
| 400 | `webgate: the colours are 2 to 256 of R G B bytes` | a bad `/palette` |
| 400 | `webgate: the pictures to make ahead are a POST of lines` | a bad `/prefetch` |
| 404 | `webgate: no aerajsc` | `/scripts` with no aerajsc |
| 501 | `webgate: snapshots need the Gateway's browser (webgate --browser)` | a snapshot without `--browser` |
| 501 | `webgate: live view needs the Gateway's browser (webgate --browser)` | live view without `--browser` |
| 504 | `webgate: the browser didn't finish HOST in 60 s` | a snapshot too slow |
| 504 | `webgate: the browser's screen didn't come: REASON` / `webgate: the browser's page didn't come: REASON` | live view too slow |
| 504 | `webgate: the Gateway's browser isn't listening (webgate-ext last asked N s ago); on the Raspberry Pi: sudo systemctl restart webgate-browser` | the browser's extension isn't there |

### Limits

| What | Limit |
|---|---|
| a picture taken in to make smaller | 40 MB, 40 megapixels |
| aerapens | 6 s; 64 MB of memory, 2 s of CPU |
| a script compiled | 4 MB; 8 s; 512 MB of memory |
| pictures made ahead | 4 at a time, 100 a list, kept 3 minutes, 48 MB in all |
| palettes kept | the last 16 |
| a page translated | 4 MB; its first 64 KB of text; 90 s |
| Cloudflare's check | 512 KB read; a visit kept 25 minutes |
| a server | 60 s for each step |
| a snapshot | 60 s; live view's screen 20 s |

### Security, log, needs

- **The browser** runs as webgate's user and goes out only through webgate,
  which checks every address: it resolves no names itself, QUIC and direct
  WebRTC are off, and its extension blocks home-network addresses and names
  (unless `--allow-local`). Pages can't reach port 8281.
- **Log** (`-v`): `ATARI_IP METHOD https://host:443 -> STATUS, N bytes` and
  what was done (`(WEBP 1200x800 to JPEG 608x405)`, `(made ahead)`,
  `(compiled, X KB to Y KB)`, `(translated fr>en in 3.2 s)`, `(a snapshot)`).
  Never the path, contents, cookies, form data or passwords.
- **Needs:** standard Python 3; Pillow; gcc and make (to build aerapens and
  aerajsc); Chromium, Xvfb and xdotool with `--browser`; `translate` for
  translation.
- **State:** the browser's profile in `/var/lib/webgate`; palettes, pictures
  made ahead and visits in memory only.

---

## pdf: EConvert and EPDF

PDFs converted on the Gateway for the Atari: the same file, byte for byte, that
EConvert writes on a Falcon with the same settings, because aerapdf is
EConvert's own converter built for the Gateway. Optionally with each page's
text translated.

- **Program:** `gateway/pdfgate.py`, running aerapdf, built from `pdf/`
  (EPDF's source at the commit in `pdf/EPDF`, copied by `tools/sync-pdf.sh`).
  The protocol is EPDF's: its `docs/PI-GATEWAY.md` has the full description,
  and `docs/CONVERTED-PDF.md` the converted PDF.
- **Port:** 8380.

### Install

```
sudo aera-gateway install pdf [--allow ATARI_IP]
```

| Option | What it does |
|---|---|
| `--port N` | another port (8380 by default) |
| `-v` / `--no-verbose` | log every request (never its query, which names the file) |

It installs gcc and make if missing and builds aerapdf (a few minutes on a
Raspberry Pi), with its fonts, in `/opt/aera-gateway-pdf`; an update builds
it again only when `pdf/` has changed. Only from AERA-Gateway itself (an
app's copy of `gateway/` has no `pdf/`).

Command line: `pdfgate.py [--port 8380] [--bind 0.0.0.0] [--allow IP] [--max-mb 64] [--time 600] [--memory-mb 768] [--jobs 1] [--tmp DIR] [-v]`.

### How the Atari talks to it

```
POST /convert?pages=mono&photos=indexed&pictures=screen&page_width=640&picture_width=640&double=1&name=REPORT.PDF HTTP/1.1
Content-Length: N

(the PDF's bytes)
```

Every option is optional, and each may be given once:

| Option | Values |
|---|---|
| `pages` | `none`, `mono`, `colour` |
| `photos` | `indexed`, `jpeg` |
| `pictures` | `page`, `screen` |
| `page_width` | 160-4096 |
| `picture_width` | 64-4096 |
| `double` | `0` (normal pages) or `1` |
| `name` | the PDF's own name, as the result records it: letters, digits, `_ . -` and spaces, up to 64 |
| `translate` | a language, two lower-case letters (`en`, `he`): each page's text also translated, by translategate on the Gateway |

While it waits its turn and while aerapdf works, the Atari gets

```
HTTP/1.1 102 Processing
X-Progress: DONE/TOTAL
```

every few seconds (`0/1000` while waiting), and then the answer.

`GET /version` answers `aerapdf EPDF_COMMIT`: which converter answers.

### Answers

200 `application/pdf` with the converted PDF. With `translate`, also
`X-Translate-From: LANG` (the language most pages were in) and
`X-Translate-Same: 1` when every page was in that language already. A page
the conversion's time didn't reach comes untranslated.

### Refusals

| Status | Text | When |
|---|---|---|
| 403 | `pdfgate: not from this computer` | the Atari isn't allowed |
| 404 | `pdfgate: no such page` | another address |
| 400 | `pdfgate: no option K` | an unknown or repeated option |
| 400 | `pdfgate: K V?` / `pdfgate: name?` / `pdfgate: translate=V` | a value it doesn't take |
| 411 | `pdfgate: no length` | no Content-Length |
| 413 | `pdfgate: the PDF is over 64 MB` | too big |
| 415 | `pdfgate: not a PDF` | it doesn't start `%PDF-` |
| 503 | `pdfgate: no aerapdf` | the converter isn't there |
| 504 | `pdfgate: the PDF took over 600 s` | too long |
| 422 | `pdfgate: no translation from LANG into CODE` | that pair of languages isn't installed |
| 503 | `pdfgate: the translation service is busy` | translategate is loading, or not there |
| 504 | `pdfgate: the translation took too long` | a page's translation took too long |
| 422 | `pdfgate: WHY` | aerapdf couldn't convert it, in its own words (the Gateway's paths taken out) |

### Limits, log, needs

- A PDF up to 64 MB; one conversion at a time; 600 s and 768 MB each;
  translation 60 s a page, ending a minute before the conversion's own time.
- The PDF and the result are on the Gateway's disk only while it converts.
- **Log:** `ATARI_IP STATUS, N bytes in, M out in T s`. Never the file's name,
  nor aerapdf's words (they may quote the PDF), nor a word of the text.
- **Needs:** standard Python 3; gcc and make at install; `translate` for
  `translate=`.
- **State:** `/opt/aera-gateway-pdf` (aerapdf, fonts, the EPDF commit).

---

## translate: translation for the apps

Text translated on the Gateway by a local engine: no text leaves the home
network, and none is kept. The full protocol is AERA-Atari's
`standards/TRANSLATION-SERVICE.md`; the apps' client is AERA-Atari's
`common/src/translate.c`.

- **Programs:** `gateway/translategate.py` (the service),
  `gateway/translatelangs.py` (the Languages job), `gateway/translateclient.py`
  (for webgate and pdfgate), `gateway/translate-languages.tsv` (the
  languages and their sizes), `gateway/translate-phrases.tsv` (greetings and
  closings).
- **Port:** 8480 for the Ataris; `/run/translategate/local.sock` for webgate and
  pdfgate on the Gateway itself.

### Install

```
sudo aera-gateway install translate [--allow ATARI_IP] [--languages en,de]
```

| Option | What it does |
|---|---|
| `--port N` | another port (8480 by default) |
| `--languages CODES` | exactly these languages (English and German by default); others are removed |
| `--engine argos\|stand-in` | Argos Translate (the default), or a stand-in without models (for tests) |
| `-v` / `--no-verbose` | log the status pages too |

The engine needs a 64-bit system (aarch64 or x86_64) and 4 GB of memory.
`install translate` puts CTranslate2 and SentencePiece in a venv of their own
(`/opt/translategate/venv`; python3-venv if missing), downloads Argos
Translate's models (each language to and from English, the others through
English; English and German are about 290 MB), and refuses languages that
wouldn't fit in 1108 MB. translategate may take 1.5 GB of memory. An update
keeps the engine and the models.

Command line: `translategate.py [--port 8480] [--bind 0.0.0.0] [--allow IP] [--engine argos|stand-in] [--models DIR] [--state DIR] [--workers 2] [--time 60] [--local PATH] [-v]`.

### How the Atari talks to it

HTTP/1.0, one request a connection, all text UTF-8.

| Request | Answer |
|---|---|
| `GET /status` | `translategate 1`, `engine argos VERSION`, `ready` (or `loading`) |
| `GET /languages` | one `CODE<TAB>NAME` line for each language installed |
| `GET /languages?all=1` | `cap_mb 1108`, `used_mb N`, then `CODE<TAB>NAME<TAB>MB<TAB>installed\|available` for each language there is |
| `POST /translate?to=CODE[&from=CODE]` | the translation, with `X-Translate-From: CODE` |
| `POST /languages` (body `he,de`) | `202 job N` (and `X-Translate-Job: N`), or `200 job 0 done` when nothing changes |
| `GET /job` | `job N`, then `running` and `bytes D of T`, `done`, `failed: WHY` or `none` |

```
POST /translate?to=en HTTP/1.0
Content-Type: text/plain; charset=utf-8
Content-Length: 19

שלום, מה שלומך?
```

- Without `from`, the language is detected. A text already in CODE answers
  200 with `X-Translate-Same: 1` and an empty body: the app keeps its own.
- **The text is shaped the same for every app:** paragraphs (blank lines)
  come back exactly; the lines of a paragraph are joined and translated as
  sentences and come back as one line, unless they are all short (under 40
  characters: an address, a list), when each line is kept; quote markers
  (`>`) are kept. Web and e-mail addresses, amounts and numbers with units
  pass the engine untouched.
- **Languages from the Atari:** the apps' Languages page sends the wanted set
  only when it closes with OK (Erez, 7 Nov 2026); a job downloads what is
  missing and removes what isn't wanted, and translategate starts again with
  the new models. Only from the Ataris allowed.
- A closed connection stops the work.
- **The local socket** takes `GET /status`, `GET /languages` and
  `POST /translate` only (up to 256 KB and 600 s), from the `aera-translate`
  group: webgate and pdfgate.

### Refusals

The body is one line, `translategate: WHY`.

| Status | WHY | When |
|---|---|---|
| 403 | `not from this computer` | the Atari isn't allowed |
| 404 | `no such page` | another address |
| 400 | `no such option` / `a language code isn't right` / `no Content-Length` / `the text isn't UTF-8` / `too long` | a request it can't read |
| 413 | `the text is over 8 KB` | too long (the apps send 2 KB at a time) |
| 422 | `that pair of languages isn't installed` (with `X-Translate-From`) | install it: Settings > Languages |
| 422 | `the translation came out too long` | the engine's answer was over 4 times the text |
| 422 | `those languages take N MB; 1108 MB fit` | a Languages request too big |
| 503 | `the engine is loading` | just started |
| 503 | `a languages job is running` / `can't start the job (REASON)` | a Languages request while one runs |
| 504 | `it took over 60 s` | too long, waiting for the engine included |
| 500 | `the engine failed` | |

A languages job that fails says why in `GET /job`: for example
`failed: the Gateway's disk has N MB free; M MB are needed`, or
`failed: Argos Translate has no model for a>b`.

### Limits, log, needs

- A text up to 8 KB (256 KB on the local socket); 60 s (600 s); two at once,
  one of them kept for short texts (a subject).
- **Log:** `ATARI_IP he>en 200, N bytes in, M out, T s`; never a word of the
  text, in the log or in an error.
- **Needs:** standard Python 3; CTranslate2 and SentencePiece in their venv.
  Only the Languages job connects anywhere: to Argos Translate's index and
  downloads, over HTTPS.
- **State:** `/opt/translategate` (the venv and the models),
  `/var/lib/translategate` (the Languages job's request and progress).

---

## radio: EPlay

Web radio for EPlay. A station may send MP3, AAC, Ogg or HLS; the Atari
always gets MP3, with the songs' titles, and can ask for a song's cover or a
station's logo.

- **Program:** `gateway/radiogate.py`, EPlay's own (its `radio/`), copied by
  `tools/sync-radio.sh` at the EPlay commit in `gateway/EPLAY`. EPlay owns
  the protocol.
- **Port:** 8580.

### Install

```
sudo aera-gateway install radio [--allow ATARI_IP]
```

| Option | What it does |
|---|---|
| `--port N` | another port (8580 by default) |
| `--allow-local` / `--no-allow-local` | also play stations on the home network |
| `-v` / `--no-verbose` | log every request |

It installs ffmpeg if it is missing (Erez, 8 Nov 2026). radiogate may take
768 MB of memory.

Command line: `radiogate.py [--port 8580] [--bind 0.0.0.0] [--allow IP] [--allow-local] [--cafile F] [--ffmpeg PATH] [--jobs 2] [--memory-mb 256] [--timeout 20] [-v]`.

### How the Atari talks to it

HTTP/1.0.

```
GET /stream?url=https%3A%2F%2Fradio.example.org%2Flive.aac HTTP/1.0
Icy-MetaData: 1
```

| What the station sends | What the Atari gets |
|---|---|
| MP3 | the MP3 as it comes, over HTTP or HTTPS |
| AAC, Ogg, or any other sound | MP3 made by ffmpeg: 128 kbit/s, 44.1 kHz, stereo |
| HLS (`.m3u8`; MPEG-TS or AAC pieces) | MP3 made by ffmpeg; the variant at 160 kbit/s or less (else the lowest) |
| a station list (`.pls`, `.m3u`) | its first station, as above |
| HLS in fragmented MP4 | refused |

- Redirects are followed, five at most.
- With `Icy-MetaData: 1`, the songs' titles come in the stream every 8192
  bytes (Icecast's way), with the station's `StreamUrl` (often a cover) when it
  sends one.

| Request | Answer |
|---|---|
| `GET /stream?url=STATION` | `200`, `Content-Type: audio/mpeg`, `icy-name`, `icy-br`, `icy-metaint: 8192` (when asked), then the stream until the Atari closes it |
| `GET /art?url=PICTURE` | the picture (a cover: JPEG, PNG, GIF, WebP...) as a 64x64 baseline JPEG of its middle square |
| `GET /logo?station=STATION&name=NAME` | the station's logo from radio-browser.info, as a 64x64 JPEG; 404 when it has none |
| `GET /version` | `radiogate 1.2, ffmpeg VERSION` |

`/logo` looks the station up in radio-browser.info, a public directory of
stations: by its address, then by its exact name, then by its name's first
words (taken only from a station on the same domain). So the Gateway sends
that directory the station's address and name (Erez, 8 Nov 2026).

### Refusals

The body is one line, `radiogate: WHY`. Once a stream has begun, a failure
only ends it.

| Status | WHY | When |
|---|---|---|
| 403 | `not from this computer` | the Atari isn't allowed |
| 404 | `no such page` | another address |
| 400 | `not a station's address` | not an http or https address, or one it won't take |
| 403 | `that station is on the home network` | without `--allow-local` |
| 503 | `too many stations at once` | two stations play already |
| 503 | `too many pictures at once` | two pictures are being made |
| 502 | `the station doesn't answer` / `the station closed the connection` / `the station answered N` / `the station fell silent` | the station |
| 502 | `the station's answer isn't one this reads` / `the station sent more than this takes` / `the station sent this from one address to another too often` | an answer it can't take |
| 502 | `the station list holds no station` | an empty `.pls` or `.m3u` |
| 502 | `this station needs ffmpeg on the Gateway` / `pictures need ffmpeg on the Gateway` | no ffmpeg |
| 502 | `the station's sound couldn't be converted` | ffmpeg couldn't read it |
| 502 | `the station's HLS has no stream` / `this station's HLS (fragmented MP4) isn't taken yet` | HLS it can't play |
| 502 | `not a picture this reads` | `/art` of something that isn't a picture |
| 502 | `the station directory doesn't answer` | `/logo`: radio-browser.info didn't answer |
| 404 | `no logo for this station` | `/logo`: none found |

### Limits, log, needs

- Two stations at once (`--jobs`); two pictures at once; 20 s for a station to
  answer or before it counts as silent.
- A station list or playlist up to 64 KB, an HLS piece 16 MB, a picture 4 MB.
- ffmpeg runs on one thread, at a low priority, with 256 MB, reading only
  what radiogate gives it.
- The last 64 covers and 256 logos are kept in memory; a station without a
  logo is asked about again after a day.
- **Log:** `ATARI_IP listened to HOST for N s, K KB (why it ended)`. Never the
  station's path or the titles.
- **Needs:** standard Python 3; ffmpeg.
- **State:** none on disk.

---

## claude: ClaudeST

ClaudeST's bridge, connecting an Atari running `CLAUDEST.PRG` to claude.ai or
the Anthropic API. It came here from ClaudeST on 9 Nov 2026 (Erez) and is
released with AERA-Gateway; ClaudeST still owns its protocol (see ClaudeST's
`PROTOCOL.md`) and reviews its changes. The Gateway sets it up as ClaudeST's
`pi/install.sh` did (`tests/systemd.sh` checks that).

- **Program:** `gateway/claude-st/bridge/claude_bridge.py` (with `backends.py`
  and `atari_text.py`; `gateway/claude-st/CLAUDEST` names the ClaudeST commit
  it came from), installed in `/opt/claude-st` and run as `claude-st.service`
  (`gateway/claude-st/claude-st.service`).
- **Port:** 2323 on the network, or a serial cable (19200 baud).

### Install

```
sudo aera-gateway install claude --network --atari ATARI_IP
sudo aera-gateway install claude --serial /dev/ttyUSB0 --baud 9600
sudo aera-gateway set-key        a new claude.ai session key
```

| Option | What it does |
|---|---|
| `--network` | over STinG (Ethernet or WiFi), on port 2323 |
| `--atari ATARI_IP[,IP2]` / `any` | the Atari's address (network mode) |
| `--listen-port N` | another port (2323 by default) |
| `--serial DEVICE` | a serial cable instead (`auto`: a USB adapter) |
| `--baud N` | the cable's speed (19200 by default) |
| `--backend claudeai\|api\|demo` | claude.ai with a session key (the default), the Anthropic API with a key, or a demo |
| `--from DIR` | the bridge from a ClaudeST copy (`bridge/` and `pi/`) instead |
| `--set-key` | paste a new claude.ai session key |

It installs python3 and python3-venv, and the bridge's packages (pyserial,
requests, anthropic, and curl_cffi where there is one) in
`/opt/claude-st/venv`: ClaudeST's own set, beside the Gateway's
standard-Python rule. The user `claude-st` is in the `dialout` group for the
cable.

### How the Atari talks to it

Lines of TAB-separated fields in the Atari's character set; the bridge
converts to and from Unicode. On the network, ClaudeST connects to
`GATEWAY_IP:2323` (Options > Settings > Network); one Atari at a time.
The requests (`HELLO`, `LIST`, `OPEN`, `NEW`, `SEND`, `ATTACH`...) and the
one-letter answers are in ClaudeST's `PROTOCOL.md`.

### Limits, log, state

- Attachments up to 500 MB; the last 40 messages of a chat are shown.
- Only the Ataris named with `--atari` (or any home-network address with
  `any`) may connect.
- **Log:** who connected, attachments' names and sizes, the model; with the
  bridge's `-v`, every line both ways.
- **State:** `/etc/claude-st/claude-st.env` (the settings and the session or
  API key; root and claude-st only) and `/var/lib/claude-st` (chats kept by the
  API backend). An update and a remove keep both.
