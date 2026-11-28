# Notes for working on AERA-Gateway

AERA-Gateway is the Raspberry Pi side of the AERA Atari apps: the gateways
for EMail (`mail`, emailgate), ECal and EContact (`dav`, ecalgate), EFTP
(`ftp`, eftpgate) and EWeb (`web`, webgate, with aerapens from `pens/`,
aerajsc from `jsc/` and the Pi's browser), the shared rules (`gateway/aeragate.py`) and the one
installer (`gateway/aera-gateway`), which also sets up ClaudeST's bridge
(`claude`).

## Who decides what

- This repository has its own owner session: the installer, the port
  table, `aeragate.py`, CI and the docs.
- Each app's session owns its gateway's protocol (emailgate's STARTTLS
  and "emailgate:" refusal lines, ecalgate's feed trimming and texts,
  eftpgate's FTP server, FTPS session reuse, SFTP and known_hosts,
  webgate and webgate-ext, aerapens' request and pens, aerajsc's request
  and bytecode (`jsc/eweb/` is EWeb's, `tools/sync-jsc.sh`), the bridge in
  ClaudeST): ask it before changing those, and send it the change to
  review.
- **A port never moves without the owning app's OK**: the Ataris have
  them in their settings (143/587/110, 8180, 2121 and 30000-30099, 8280,
  8580, 8581, 2323; 8281 is webgate's extension's and 3678 spotgate's
  player's, on 127.0.0.1 only).
- ClaudeST's bridge stays in ClaudeST; the `claude` recipe sets it up as
  ClaudeST's `pi/install.sh` did, and `tests/systemd.sh` checks that.
  - **The Gateway session owns the gateway as one unit** (decided 7 Nov
  2026; guidelines 7.3, AERA-Atari 49e1d9f). It commits, tests and
  releases everything that runs on the Pi: the services, the installer,
  `aeragate.py`, the port table, CI, the one `VERSION` and
  `update-pi.sh`. Nobody else pushes AERA-Gateway, the hub included.
  The app sessions decide their service's protocol and write its code,
  and hand it over as a `git format-patch` with tests, or as a
  `tools/sync-NAME.sh` copy of a named commit of their repository (never
  edited here). This session reviews each against the unit (install,
  what an update keeps, remove, `tests/systemd.sh`, the ports, guideline
  7.3, standard Python only, one `VERSION`), integrates it, runs the
  checks, releases it and sends the link to the app session and the
  hub. An app release that needs a gateway change names that version
  and goes out after it.

## Talking with the owner

Write to them only when they have to decide something, with a short context:
no progress or in-between messages. Analysis is the information, your
assessment and what they should do. (Messages to the hub and the app
sessions stay as they are.)

## Rules

- **The apps carry `gateway/` as a read-only copy**: a subtree of the
  `gateway` branch, which is `git subtree split --prefix=gateway main`.
  After a change here: push main, then
  `git subtree split --prefix=gateway -b gateway && git push origin gateway`,
  and tell the app sessions to run `tools/sync-gateway.sh`.
- **Standard Python 3 only** (3.9, Raspberry Pi OS bullseye, and newer),
  plus paramiko for eftpgate's SFTP and Pillow (python3-pil) for
  webgate's pictures; `install web --browser` adds Chromium, Xvfb and
  xdotool (decided 20 Oct 2026); `install web` adds gcc and
  make, if missing, to build aerapens from `pens/` (21 Oct 2026) and
  aerajsc from `jsc/` (26 Oct 2026); `install spotify` adds ffmpeg,
  libasound2 and go-librespot's pinned release, sha256 checked, in
  /opt/spotgate (28 Nov 2026). No other packages on the Pi.
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
  yourself only on a throwaway computer. A change to the docs alone
  (README, docs/, CLAUDE.md) takes the short path: pushed once it reads
  right, without the test runs (guidelines 1.2).
- **Only formal releases leave the build machine.** The owner gets vX.Y
  releases made by the Release workflow from `main`, nothing else: no
  test builds, no dev builds, no builds from a branch. Work lands on
  `main`; to try a fix on real hardware, release it. If a release doesn't
  work, fix forward: a new release with the next number, before any other
  work, with one line in its notes saying what it fixes; the old release
  stays as it is (no revert, no "withdrawn" notice, no deleting;
  guidelines 1.2, 7 Nov 2026). Here a release is `VERSION` in `gateway/aera-gateway`
  and Actions > Release > Run workflow; the Pi follows the latest one
  (`update-pi.sh`), and ClaudeST's bridge comes from ClaudeST's latest
  release.
  - **Fewer releases, each carrying more** (decided 26 Nov 2026;
  guidelines 1.2, AERA-Atari 9da7632; it replaces 23 Oct's "release as
  soon as the work is done"). Changes gather on `main` and go out
  together when a body of work is complete: the fixes known at the time
  and what the apps need, as one candidate through the regression gate
  and the hub's go. An app's protocol change waits for that release,
  unless an app's release needs it sooner.
    - The one exception goes out on its own at once, with only that fix
      and its notes saying why: a fault that loses or damages data, a
      security fault, or a crash or breakage that stops users working.
    - A fault a user reports is fixed with its test, and rides the next
      release unless it is one of those.
    - The hub's shared fixes ride the next release unless the hub names
      one as the exception.
    - **Nothing known is left in a queue behind a release** (decided 26
      Nov 2026; guidelines 1.2, AERA-Atari bf0b8d5, c8429a7). Before a
      release, fold in every known fault and pending item of this
      repository's that can be done within a few days; the candidate's
      message to the hub lists what was left out and why, and the hub
      checks it before its go. A reported fault that is waiting goes into
      the release then being prepared, not the one after. In maintenance
      mode this decides what a release carries, not whether there is one.
    - Releases go out without asking the owner (decided 23 Oct 2026).
      Then give the owner the link in one line, with anything they have
      to do (on the Pi, `./update-pi.sh`), and send the hub the link.
      Fix a bad release forward the same way. Decisions that are theirs
      still go to them first.
  A change to the docs alone needs no release of its own: it goes out
  with the next one.
  - **The regression gate** (decided 17 Nov 2026: the software has users, so
  every configuration is tested and no performance is lost; guidelines 1.2, AERA-Atari a22574e). Every release,
  before it goes out:
    1. pyflakes, the unit tests, and CI with `tests/systemd.sh`;
    2. each service the change touches tested by its owning app against
       the candidate (`make itest` or `gateway.py`, from a `gateway/` copy
       of the candidate commit): EMail, ECal, EContact, EFTP, EWeb, EPDF,
       EPlay, ClaudeST's bridge;
    3. the update path, an update keeping everything (`tests/systemd.sh`),
       on Raspberry Pi OS bullseye and the newer one (CI's Debian bullseye
       and bookworm runs on arm64);
    4. the candidate and the results to the hub, which answers go or
       faults; release on its go.

    A fault a user reports becomes a test in the release that fixes it.
- **Who decides what** (decided 31 Oct 2026; the guidelines' "Who decides
  what", AERA-Atari 797fddf). The owner's part, put to them before the work as
  a decision with a short context:
  - new programs and ideas, their core capabilities, and how they work
    with the other programs;
  - the user experience: the look, the layout, the way through a task,
    the words, the icons, the defaults;
  - phases: work stays inside the phase they approved; at its end, one
    phase report (what it set out to do, what was done with the release
    links, what is left, the next phase as a proposal), and the next
    phase starts when they say so;
  - priorities and conflicts between programs or sessions (through the
    hub, with its options and recommendation);
  - packages on the Pi and ports.

  How it is made is the team's, without asking: code, structure, shared
  code, tests, measurements, fixes, docs and releases inside an approved
  phase.
- **The owner is not named as the one who asked or decided** anywhere on
  GitHub (AERA-Atari, standards/GITHUB-IDENTITY.md): commit messages,
  release notes, comments, code and docs say what was decided and when
  ("decided <date>"), never who. The author and copyright lines keep
  "AERA | Erez Yaary". Older text is cleaned when its file is next touched.
- **Never commit:** real addresses from the owner's network (use
  `192.168.1.x`, `PI_IP`, `ATARI_IP`), passwords, session keys; nor,
  on any branch, anything the owner gives a session (their files, the
  Pi's logs, screenshots, share links or file IDs, their messages and
  words, their machines' settings), in files, commit messages, test
  names or notes. A fault they report becomes a case made from scratch
  that shows the same thing (AERA-Atari standards/GITHUB-IDENTITY.md,
  Commits).
- British English, as in the app READMEs.
- **The name** reads "AERA | Erez Yaary" wherever it is given: copyright
  lines (`Copyright (C) 2026 AERA | Erez Yaary`), the docs, and as the
  author of every commit:
  `git commit --author="AERA | Erez Yaary <yaary@me.com>"`, with Claude as
  the signing committer (AERA-Atari, `standards/GITHUB-IDENTITY.md`).

## Maintenance mode (decided 1 Nov 2026; 17 Nov 2026)

AERA-Gateway is in maintenance mode while EWeb, EPDF and EPlay go to 1.0 (AERA-Atari, ATARI-APP-GUIDELINES.md, section 8). Until the owner ends it:
- No new features, clean-ups or installer work of its own.
- Every family rule holds for it as for the focus apps (decided 17 Nov 2026: an app in maintenance is held to the same rules): 7.3, 1.2's release rules, the log rules and every other. The hub notifies this session of each new or changed rule; the session adopts it and reports to the hub, which ticks its row in section 8. Whether that needs a release of its own is reviewed with the owner through the hub; a fault fix always gets one.
- What EWeb, EPDF or EPlay ask for (webgate, aerapens, aerajsc, pdfgate, radiogate, spotgate) goes on as before, committed and released by this session.
- Otherwise a release only for:
  - a security or data-loss fault;
  - a crash or breakage;
  - something outside that stops it working (Raspberry Pi OS, a certificate, a server).

  Such a release takes only that fix, and its notes say why.
- Nothing runs in the background; the session waits for the owner, the hub or a focus app's session.
- Only the owner's explicit word ends this mode. The hub passes it on quoting them.
