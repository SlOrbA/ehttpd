#!/bin/sh
set -eu

PORT=18080
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
  if wget -qO- "http://127.0.0.1:$PORT/" > /tmp/ehttpd-test.html 2>/dev/null; then
    break
  fi
  i=$((i + 1))
  sleep 1
done

test -s /tmp/ehttpd-test.html
grep -q '<html>' /tmp/ehttpd-test.html
grep -q 'Ready serve requests' /tmp/ehttpd-test.err
