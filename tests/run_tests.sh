#!/bin/sh
set -eu

if ! command -v wget >/dev/null 2>&1; then
  echo "SKIP: wget is required for the HTTP smoke test" >&2
  exit 77
fi

SERVER="../source/ehttpd"
TEST_TMPDIR=$(mktemp -d "${TMPDIR:-/tmp}/ehttpd-test.XXXXXX")
SERVER_PID=

cleanup() {
  if [ -n "$SERVER_PID" ]; then
    kill "$SERVER_PID" 2>/dev/null || true
    wait "$SERVER_PID" 2>/dev/null || true
  fi
  rm -rf "$TEST_TMPDIR"
}
trap cleanup EXIT INT TERM

# Try several high ports. EHTTPD_TEST_PORT is useful when a caller needs to
# select a specific port, while normal runs retry if the derived port is busy.
PORT_BASE=${EHTTPD_TEST_PORT:-$((49152 + ($$ % 10000)))}
attempt=0
while [ "$attempt" -lt 20 ]; do
  if [ -n "${EHTTPD_TEST_PORT:-}" ]; then
    PORT="$EHTTPD_TEST_PORT"
  else
    PORT=$((PORT_BASE + attempt))
  fi
  "$SERVER" 4 "$PORT" >"$TEST_TMPDIR/server.out" 2>"$TEST_TMPDIR/server.err" &
  SERVER_PID=$!
  sleep 1
  if kill -0 "$SERVER_PID" 2>/dev/null; then
    break
  fi
  wait "$SERVER_PID" 2>/dev/null || true
  SERVER_PID=
  attempt=$((attempt + 1))
done

test -n "$SERVER_PID"
i=0
while [ "$i" -lt 20 ]; do
  if wget -T 1 -qO- "http://127.0.0.1:$PORT/" > "$TEST_TMPDIR/response.html" 2>/dev/null; then
    break
  fi
  i=$((i + 1))
  sleep 1
done

test -s "$TEST_TMPDIR/response.html"
grep -q '<html>' "$TEST_TMPDIR/response.html"
grep -q 'Ready serve requests' "$TEST_TMPDIR/server.err"
