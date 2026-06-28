#!/usr/bin/env bash
# Integration test script for the C++ backend API.
# Run from backend/ after: make

set -euo pipefail

BASE="http://localhost:8080"
SERVER="./build/finance_server"
EMAIL="testuser_$(date +%s)@ku.edu.np"
PASS="secret123"

cleanup() {
  if [[ -n "${SERVER_PID:-}" ]]; then
    kill "$SERVER_PID" 2>/dev/null || true
  fi
}
trap cleanup EXIT

if [[ ! -x "$SERVER" ]]; then
  echo "Build the server first: make"
  exit 1
fi

mkdir -p data
rm -f data/store.dat

./build/finance_server &
SERVER_PID=$!
sleep 1

echo "== Health =="
curl -sf "$BASE/api/health" | grep -q '"status":"ok"'
echo "OK"

echo "== Register (zero expenses) =="
REGISTER=$(curl -sf -X POST "$BASE/api/auth/register" \
  -H "Content-Type: application/json" \
  -d "{\"email\":\"$EMAIL\",\"password\":\"$PASS\",\"isStudent\":true,\"inHostel\":true,\"budget\":15000}")
echo "$REGISTER" | grep -q '"success":true'
TOKEN=$(echo "$REGISTER" | sed -n 's/.*"token":"\([^"]*\)".*/\1/p')
echo "$REGISTER" | grep -q '"spent":0'
echo "Token: ${TOKEN:0:20}..."

echo "== Add expense =="
ADD=$(curl -sf -X POST "$BASE/api/expenses" \
  -H "Content-Type: application/json" \
  -H "Authorization: Bearer $TOKEN" \
  -d '{"name":"Momo","amount":140,"category":"food"}')
echo "$ADD" | grep -q '"success":true'
echo "$ADD" | grep -q '"topItemName":"Momo"'

echo "== Dashboard =="
DASH=$(curl -sf "$BASE/api/dashboard" -H "Authorization: Bearer $TOKEN")
echo "$DASH" | grep -q '"spent":140'
echo "$DASH" | grep -q '"count":1'

echo "== Login =="
LOGIN=$(curl -sf -X POST "$BASE/api/auth/login" \
  -H "Content-Type: application/json" \
  -d "{\"email\":\"$EMAIL\",\"password\":\"$PASS\"}")
echo "$LOGIN" | grep -q '"success":true'

echo ""
echo "All API integration tests passed."
