#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
BIN="$ROOT/build/bin"
exec "$BIN/harness" "$BIN/subconvd" "$BIN/mock_upstream" "$ROOT/tests/fixtures/integration/sub.txt" "$ROOT/tests/fixtures/singbox/example.json" "$ROOT/tests/fixtures/expected/singbox.yaml"
