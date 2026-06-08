#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

YYJSON="$ROOT/.build/source/yyjson/src"
if [ ! -f "$YYJSON/yyjson.c" ]; then
  echo "ast-test: yyjson not fetched; run 'make fetch' first" >&2
  exit 1
fi

OUT="$(mktemp -d)"
trap 'rm -rf "$OUT"' EXIT

bun run tools/gen-ast.ts

zig cc -std=c11 -g -O0 -Wall -Wextra -Wno-unused-parameter \
  -Iinclude -Isrc -Ivendor -I"$YYJSON" \
  test/ast_test.c src/ast/ast.c "$YYJSON/yyjson.c" \
  -o "$OUT/ast_test"

"$OUT/ast_test"
