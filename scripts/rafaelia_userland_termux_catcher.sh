#!/usr/bin/env bash
# RAFAELIA UserLAnd Termux Catcher
# Passive read-only anchor checker.
set -u
status=0
ok(){ printf '[OK] %s\n' "$1"; }
miss(){ printf '[MISS] %s\n' "$1"; status=1; }
file(){ [ -f "$1" ] && ok "$1" || miss "$1"; }
dir(){ [ -d "$1" ] && ok "$1/" || miss "$1/"; }

printf '== RAFAELIA UserLAnd Termux Catcher ==\n'
printf 'mode=passive-read-only\n\n'
