#!/bin/bash
# Test authentication and token sending
echo "Testing auth server..."
echo "AUTH Stephen Password" | nc localhost 3001
echo ""
echo "Getting token..."
TOKEN=$(echo "AUTH Stephen Password" | nc localhost 3001 | awk '{print $2}')
echo "Token: $TOKEN"
echo ""
echo "Testing chat server with token..."
(echo "TOKEN:$TOKEN"; sleep 1) | nc localhost 3000
