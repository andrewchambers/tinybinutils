#!/bin/sh

set -eu

if ! command -v nix-build >/dev/null 2>&1; then
	echo "FAIL"
	echo "error: nix-build not found; please install Nix" >&2
	exit 1
fi

cd "$(dirname "$0")/nix"
log=$(mktemp)
trap 'rm -f "$log"' EXIT

if nix-build tests.nix -A all --out-link result >"$log" 2>&1; then
	echo "PASS"
else
	echo "FAIL"
	cat "$log" >&2
	exit 1
fi
