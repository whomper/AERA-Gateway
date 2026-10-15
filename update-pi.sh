#!/bin/bash
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
# Bring every AERA gateway on this Pi up to date: mail, dav, ftp, and
# ClaudeST's bridge from ClaudeST's latest code.
#
#   ./update-pi.sh                    in this AERA-Gateway checkout, on the Pi
#   ./update-pi.sh --claudest DIR     ClaudeST's checkout is somewhere else
#                                     (default: ClaudeST next to this folder)
#
# It pulls this checkout and ClaudeST's (cloning ClaudeST if it isn't
# there), then runs  sudo gateway/aera-gateway update --from CLAUDEST,
# which keeps every setting, key and chat. A new gateway only replaces the
# old one once it answers; otherwise the old one comes back. Only what is
# already installed is updated: to add a service, see README.md.
set -euo pipefail

CLAUDEST_URL=https://github.com/whomper/ClaudeST

say() { printf '%s\n' "$*"; }
die() { printf 'update-pi: %s\n' "$*" >&2; exit 1; }

# git as the folder's owner, so that sudo ./update-pi.sh leaves no files of
# root's in a user's checkout
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

# a checkout brought up to date; never over changes of one's own
pull() {
  local dir=$1 name=$2
  local changes
  [ -d "$dir/.git" ] || die "$dir isn't a git checkout of $name"
  changes=$(git_in "$dir" status --porcelain --untracked-files=no) || die "git can't read $dir"
  if [ -n "$changes" ]; then
    die "$dir has changes of its own; commit or put them away (git stash) and run me again"
  fi
  say "== $name: $(git_in "$dir" rev-parse --short HEAD), pulling"
  git_in "$dir" pull --ff-only --quiet || die "can't bring $dir up to date with git pull --ff-only"
  say "   now $(git_in "$dir" log -1 --format='%h %s')"
}

main() {
  local here claudest=""
  here=$(cd "$(dirname "$0")" && pwd)
  while [ $# -gt 0 ]; do
    case $1 in
      --claudest) [ $# -ge 2 ] || die "--claudest wants a folder"; claudest=$2; shift 2 ;;
      -h|--help) sed -n '4,15p' "$0"; exit 0 ;;
      *) die "$1: the only option is --claudest DIR (see --help)" ;;
    esac
  done
  [ -x "$here/gateway/aera-gateway" ] || die "run me from an AERA-Gateway checkout ($here has no gateway/aera-gateway)"
  claudest=${claudest:-$(dirname "$here")/ClaudeST}

  for prog in git python3; do
    command -v "$prog" >/dev/null || { say "== Installing $prog"; sudo apt-get update -qq; sudo apt-get install -y -qq "$prog"; }
  done

  pull "$here" AERA-Gateway
  if [ -e "$claudest" ]; then
    pull "$claudest" ClaudeST
  else
    say "== ClaudeST: cloning into $claudest"
    git_in "$(dirname "$claudest")" clone --quiet "$CLAUDEST_URL" "$claudest"
    say "   now $(git_in "$claudest" log -1 --format='%h %s')"
  fi

  say ""
  if [ "$(id -u)" -eq 0 ]; then
    "$here/gateway/aera-gateway" update --from "$claudest"
  else
    sudo "$here/gateway/aera-gateway" update --from "$claudest"
  fi
  say ""
  "$here/gateway/aera-gateway" status
}

# all of it read before any of it runs: the pull above may rewrite this file
main "$@"
