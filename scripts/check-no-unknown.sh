#!/usr/bin/env bash
# Fails if the `unknown` type keyword appears in any package source or test.
set -euo pipefail
cd "$(dirname "$0")/.."
if grep -rnE '(:|<|,|\(|\|)\s*unknown\b' packages/*/src packages/*/tests --include='*.ts' ; then
  echo "error: 'unknown' type is banned (see CLAUDE.md)" >&2
  exit 1
fi
echo "ok: no 'unknown' types"
