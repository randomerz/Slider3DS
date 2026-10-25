#!/bin/sh
# A CIA's title version as its TMD stores it (major.minor.micro), or a check of it:
#   tools/cia_version.sh <file.cia> [expected]   (exit 1 if it differs)
# The Makefile's `cia` checks what makerom wrote: makerom takes -minor and writes 0 there.
cia=$1
bytes() { od -An -tu1 -j "$1" -N "$2" "$cia" | tr -s ' \n' ' '; }
le32() { set -- $(bytes "$1" 4); echo $(( $1 | $2 << 8 | $3 << 16 | $4 << 24 )); }
be() { v=0; for b in $(bytes "$1" "$2"); do v=$(( v * 256 + b )); done; echo $v; }
align() { echo $(( ($1 + 63) / 64 * 64 )); }
# The header, the certificate chain and the ticket, each 64-byte aligned, then the TMD.
tmd=$(( $(align "$(le32 0)") + $(align "$(le32 8)") + $(align "$(le32 12)") ))
sig=$(be $tmd 4)
if [ "$sig" != $((0x10004)) ]; then echo "$cia: TMD signature type $sig, expected RSA-2048 (65540)" >&2; exit 1; fi
v=$(be $((tmd + 0x140 + 0x9C)) 2) # (the TMD body after its signature; the title version)
version="$((v >> 10)).$(((v >> 4) & 63)).$((v & 15))"
if [ -z "$2" ]; then echo "$version"; exit 0; fi
if [ "$version" != "$2" ]; then echo "$cia: title version $version, expected $2" >&2; exit 1; fi
