#!/bin/sh
set -eu

if ! command -v wget >/dev/null 2>&1 || ! command -v nc >/dev/null 2>&1; then
  echo "SKIP: wget and nc are required for the HTTP tests" >&2
  exit 77
fi

SERVER=$(cd ../source && pwd)/ehttpd
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

cd "$TEST_TMPDIR"

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

request() {
  printf '%b' "$1" | nc -w 2 127.0.0.1 "$PORT"
}

printf 'original value\n' > "$TEST_TMPDIR/settings.txt"
response=$(request 'HEAD /settings.txt HTTP/1.1\r\nHost: example\r\n\r\n')
printf '%s\n' "$response" | grep -q 'HTTP/1.1 200 OK'
printf '%s\n' "$response" | grep -q 'Content-Length:'
if printf '%s\n' "$response" | grep -q '<html>'; then
  exit 1
fi

wget -T 1 -qO "$TEST_TMPDIR/edit.html" \
  "http://127.0.0.1:$PORT/settings.txt?action=edit"
grep -q "method='post'" "$TEST_TMPDIR/edit.html"

body='content=changed+value'
response=$(request "POST /settings.txt?action=edit HTTP/1.1\r\nHost: example\r\nContent-Length: ${#body}\r\n\r\n$body")
printf '%s\n' "$response" | grep -q 'HTTP/1.1 200 OK'
test "$(cat "$TEST_TMPDIR/settings.txt")" = 'changed value'
