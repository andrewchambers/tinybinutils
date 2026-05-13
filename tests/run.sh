#!/bin/sh

set -eu

if ! command -v nix-instantiate >/dev/null 2>&1; then
	echo "FAIL"
	echo "error: nix-instantiate not found; please install Nix" >&2
	exit 1
fi

cd "$(dirname "$0")/nix"
mkdir -p .gcroots
log=$(mktemp)
trap 'rm -f "$log"' EXIT

if nix-instantiate tests.nix -A all --add-root .gcroots/tests.drv --indirect >"$log" 2>&1; then
	echo "PASS"
else
	echo "FAIL"
	cat "$log" >&2
	exit 1
fi
