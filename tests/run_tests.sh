#!/bin/sh
set -eu

if ! command -v wget >/dev/null 2>&1; then
  echo "SKIP: wget is required for the HTTP smoke test" >&2
  exit 77
fi

# Set EHTTPD_TEST_PORT to force a port; otherwise derive one from the test PID.
PORT=${EHTTPD_TEST_PORT:-$((49152 + ($$ % 10000)))}
SERVER="../source/ehttpd"

"$SERVER" 4 "$PORT" >/tmp/ehttpd-test.out 2>/tmp/ehttpd-test.err &
SERVER_PID=$!
cleanup() {
  kill "$SERVER_PID" 2>/dev/null || true
  wait "$SERVER_PID" 2>/dev/null || true
}
trap cleanup EXIT INT TERM

i=0
while [ "$i" -lt 20 ]; do
  if wget -T 1 -qO- "http://127.0.0.1:$PORT/" > /tmp/ehttpd-test.html 2>/dev/null; then
    break
  fi
  i=$((i + 1))
  sleep 1
done

test -s /tmp/ehttpd-test.html
grep -q '<html>' /tmp/ehttpd-test.html
grep -q 'Ready serve requests' /tmp/ehttpd-test.err
