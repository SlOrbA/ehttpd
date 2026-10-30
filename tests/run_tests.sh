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

PORT_BASE=${EHTTPD_TEST_PORT:-$((49152 + ($$ % 10000)))}
attempt=0
cd "$TEST_TMPDIR"
while [ "$attempt" -lt 20 ]; do
  if [ -n "${EHTTPD_TEST_PORT:-}" ]; then
    PORT="$EHTTPD_TEST_PORT"
  else
    PORT=$((PORT_BASE + attempt))
  fi
  "$SERVER" 4 "$PORT" >server.out 2>server.err &
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

request() {
  printf '%b' "$1" | nc -w 2 127.0.0.1 "$PORT"
}

assert_status() {
  response=$(request "$1")
  printf '%s\n' "$response" | grep -q "HTTP/1.1 $2"
}

wget -T 1 -qO response.html "http://127.0.0.1:$PORT/"
test -s response.html
grep -q '<html>' response.html

assert_status 'GET / HTTP/1.1\r\n\r\n' '400 Bad Request'
assert_status 'GET / HTTP/1.1 extra\r\nHost: example\r\n\r\n' '400 Bad Request'
assert_status 'GET / HTTP/1.1\r\nHost: example\r\nBad Header: value\r\n\r\n' '400 Bad Request'
assert_status 'GET / HTTP/1.1\r\nHost: one\r\nHost: two\r\n\r\n' '400 Bad Request'

response=$(request 'POST / HTTP/1.1\r\nHost: example\r\n\r\n')
printf '%s\n' "$response" | grep -q 'HTTP/1.1 405 Method Not Allowed'
printf '%s\n' "$response" | grep -q 'Allow: GET'

assert_status 'GET / HTTP/1.0\r\nHost: example\r\n\r\n' '505 HTTP Version Not Supported'

printf 'original\n' > protected.txt
assert_status 'POST /choice?edit=/protected.txt&line=0&repl=modified HTTP/1.1\r\nHost: example\r\n\r\n' '405 Method Not Allowed'
test "$(cat protected.txt)" = 'original'
grep -q 'Ready serve requests' server.err
