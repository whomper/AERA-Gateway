#!/bin/bash
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
# Bring every AERA gateway on this Pi up to date, from releases only:
# this copy goes to AERA-Gateway's latest release, and aera-gateway update
# brings mail, dav, ftp and ClaudeST's bridge (from ClaudeST's latest
# release) up to date, every setting, key and chat kept.
#
#   ~/AERA-Gateway/update-pi.sh
#   ~/AERA-Gateway/update-pi.sh --allow ATARI_IP [--mail IMAP SMTP[:587] [POP3]] [--browser]
#
# With options, it installs every service for that Atari as well
# (aera-gateway install all; mail only with --mail): on a new Pi,
#   sudo apt install -y git && git clone https://github.com/whomper/AERA-Gateway ~/AERA-Gateway &&
#   ~/AERA-Gateway/update-pi.sh --allow ATARI_IP --mail IMAP SMTP
# A new gateway only replaces the old one once it answers; otherwise the
# old one comes back. Without options, only what is installed is updated.
# It also makes `aera-gateway` a command (/usr/local/bin), so that
# `sudo aera-gateway status` works from any folder.
set -euo pipefail

say() { printf '%s\n' "$*"; }
die() { printf 'update-pi: %s\n' "$*" >&2; exit 1; }

# git as the folder's owner, so that sudo ./update-pi.sh leaves no files of
# root's in a user's copy
git_in() {
  local dir=$1; shift
  local owner
  owner=$(stat -c %U "$dir")
  if [ "$(id -u)" -eq 0 ] && [ "$owner" != root ]; then
    sudo -u "$owner" -H git -C "$dir" "$@"
  else
    git -C "$dir" "$@"
  fi
}

main() {
  local here changes now latest
  here=$(cd "$(dirname "$0")" && pwd)
  case ${1:-} in
    -h|--help) sed -n '4,19p' "$0"; exit 0 ;;
    ""|--allow|--allow=*|--mail|--browser|--from) ;;
    *) die "$1: the options are those of aera-gateway install all (see --help)" ;;
  esac
  [ -x "$here/gateway/aera-gateway" ] || die "run me from an AERA-Gateway copy ($here has no gateway/aera-gateway)"
  [ -d "$here/.git" ] || die "$here isn't a git copy of AERA-Gateway (git clone https://github.com/whomper/AERA-Gateway)"

  for prog in git python3; do
    command -v "$prog" >/dev/null || { say "== Installing $prog"; sudo apt-get update -qq; sudo apt-get install -y -qq "$prog"; }
  done

  # this copy at the latest release; never over changes of one's own
  changes=$(git_in "$here" status --porcelain --untracked-files=no) || die "git can't read $here"
  [ -z "$changes" ] || die "$here has changes of its own; put them away (git stash) and run me again"
  git_in "$here" fetch --quiet --tags --force origin || die "can't fetch AERA-Gateway's releases from GitHub"
  latest=$(git_in "$here" tag -l 'v[0-9]*.[0-9]*' --sort=-v:refname | head -n 1)
  [ -n "$latest" ] || die "AERA-Gateway has no release yet"
  now=$(git_in "$here" describe --tags --exact-match 2>/dev/null || git_in "$here" rev-parse --short HEAD)
  if [ "$now" = "$latest" ]; then
    say "== AERA-Gateway $latest, the latest release"
  else
    git_in "$here" -c advice.detachedHead=false checkout --quiet "$latest" || die "can't switch $here to $latest"
    say "== AERA-Gateway: $now -> $latest, the latest release"
  fi

  # from here on, the installer of that release; and `aera-gateway`, a
  # command from any folder (a link: it follows this copy)
  local as=()
  [ "$(id -u)" -eq 0 ] || as=(sudo)
  "${as[@]}" ln -sfn "$here/gateway/aera-gateway" /usr/local/bin/aera-gateway
  say ""
  if [ $# -gt 0 ]; then
    "${as[@]}" "$here/gateway/aera-gateway" install all "$@"
  else
    "${as[@]}" "$here/gateway/aera-gateway" update
  fi
  say ""
  "$here/gateway/aera-gateway" status
}

# all of it read before any of it runs: the checkout above may rewrite this file
main "$@"
