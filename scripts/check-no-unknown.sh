#!/usr/bin/env bash
# Fails if the `unknown` type keyword appears in any package source or test.
set -euo pipefail
cd "$(dirname "$0")/.."
shopt -s nullglob
dirs=(packages/*/src packages/*/tests)
if [ ${#dirs[@]} -eq 0 ]; then
  echo "ok: no 'unknown' types"
  exit 0
fi
status=0
grep -rnE '(:|<|,|\(|\||&|\[|=>|=|\bas|\bextends)\s*unknown\b' "${dirs[@]}" \
  --include='*.ts' --include='*.mts' --include='*.cts' --include='*.tsx' || status=$?
if [ "$status" -eq 0 ]; then
  echo "error: 'unknown' type is banned (see CLAUDE.md)" >&2
  exit 1
elif [ "$status" -ne 1 ]; then
  echo "error: grep failed with status $status" >&2
  exit "$status"
fi
echo "ok: no 'unknown' types"
