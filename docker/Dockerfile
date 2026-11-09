# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
#
# The Gateway in a Docker container (Erez, 9 Nov 2026): every service, as
# `aera-gateway install` sets them up on a Raspberry Pi, started by
# docker/aera-gateway-run instead of systemd. Built from the repository's
# root:
#
#   docker build -f docker/Dockerfile -t aera-gateway .
#   docker build -f docker/Dockerfile --build-arg BROWSER=1 -t aera-gateway:browser .
#
# Linux arm64 (a 64-bit Raspberry Pi OS) and amd64. docker/compose.yml runs
# it; docs/DOCKER.md says how, and on which systems.

FROM debian:trixie-slim AS build
RUN apt-get update -qq && apt-get install -y -qq --no-install-recommends gcc make libc6-dev binutils \
    && rm -rf /var/lib/apt/lists/*
COPY pdf /src/pdf
COPY pens /src/pens
COPY jsc /src/jsc
COPY gateway/aera-gateway /src/gateway/aera-gateway
RUN V=$(sed -n 's/^VERSION = "\([0-9.]*\)".*/\1/p' /src/gateway/aera-gateway) \
    && rm -f /src/pdf/aerapdf /src/pens/aerapens /src/jsc/aerajsc \
    && make -s -C /src/pdf -j"$(nproc)" \
    && make -s -C /src/pens PENS_BUILD="v$V" \
    && make -s -C /src/jsc -j"$(nproc)" \
    && echo "$V" > /src/VERSION

FROM debian:trixie-slim
ARG BROWSER=0
# Python and the packages the services take on a Raspberry Pi (guideline
# 7.3): Pillow for webgate, paramiko for eftpgate's SFTP, ffmpeg for
# radiogate; setpriv runs each service as its own user
RUN apt-get update -qq && apt-get install -y -qq --no-install-recommends \
        python3 python3-venv python3-pil python3-paramiko ffmpeg ca-certificates util-linux \
    && if [ "$BROWSER" = 1 ]; then apt-get install -y -qq --no-install-recommends \
        chromium xvfb xdotool fonts-liberation; fi \
    && rm -rf /var/lib/apt/lists/*

# the users, as on a Raspberry Pi; aera-translate may use translategate's socket
RUN for u in emailgate ecalgate eftpgate webgate pdfgate translategate radiogate aerahelper; do \
        useradd --system --no-create-home --shell /usr/sbin/nologin "$u"; done \
    && useradd --system --home-dir /var/lib/claude-st --shell /usr/sbin/nologin claude-st \
    && groupadd --system aera-translate \
    && for u in webgate pdfgate translategate; do usermod -a -G aera-translate "$u"; done

# translate's engine and ClaudeST's bridge, each in its venv
RUN python3 -m venv /opt/translategate/venv \
    && /opt/translategate/venv/bin/pip install -q --no-cache-dir "ctranslate2>=4,<5" "sentencepiece>=0.2,<0.3" \
    && python3 -m venv /opt/claude-st/venv \
    && /opt/claude-st/venv/bin/pip install -q --no-cache-dir requests anthropic \
    && (/opt/claude-st/venv/bin/pip install -q --no-cache-dir curl_cffi || true)

COPY gateway /opt/aera-gateway
RUN mkdir -p /opt/claude-st/bridge \
    && cp /opt/aera-gateway/claude-st/bridge/*.py /opt/aera-gateway/claude-st/bridge/requirements.txt \
        /opt/claude-st/bridge/ \
    && rm -f /opt/claude-st/bridge/test_bridge.py /opt/aera-gateway/test_*.py \
    && rm -rf /opt/aera-gateway/claude-st
COPY --from=build /src/VERSION /opt/aera-gateway/VERSION
COPY --from=build /src/pdf/aerapdf /opt/aera-gateway-pdf/aerapdf
COPY --from=build /src/pdf/EPDF /opt/aera-gateway-pdf/SOURCE
COPY --from=build /src/pdf/epdf/third_party/liberation/fonts/ /opt/aera-gateway-pdf/fonts/
COPY --from=build /src/pens/aerapens /opt/aera-gateway-pens/aerapens
COPY --from=build /src/jsc/aerajsc /opt/aera-gateway-jsc/aerajsc
COPY docker/aera-gateway-run /usr/local/bin/aera-gateway-run

# what is kept lives in /data (a volume); the programs find it where they
# would on a Raspberry Pi
RUN mkdir -p /data /etc /var/lib \
    && ln -s /data/eftpgate /var/lib/eftpgate \
    && ln -s /data/claude-st/state /var/lib/claude-st \
    && ln -s /data/claude-st/etc /etc/claude-st \
    && ln -s /data/translate /var/lib/translategate \
    && ln -s /data/translate/models /opt/translategate/models \
    && ln -s /data/webgate /var/lib/webgate \
    && chmod 755 /opt/aera-gateway/*.py /usr/local/bin/aera-gateway-run \
    && chmod 644 /opt/aera-gateway/aeragate.py

VOLUME /data
# mail 143 587 110, dav 8180, ftp 2121 and 30000-30099, web 8280, pdf 8380,
# translate 8480, radio 8580, ClaudeST 2323
EXPOSE 143 587 110 8180 2121 30000-30099 8280 8380 8480 8580 2323
STOPSIGNAL SIGTERM
CMD ["/usr/local/bin/aera-gateway-run"]
