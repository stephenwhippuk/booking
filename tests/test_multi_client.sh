#!/bin/bash
# Multi-client test - Run multiple clients to test room updates

BUILD_DIR="$(cd "$(dirname "$0")/build" && pwd)"
cd "$BUILD_DIR"

GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

echo -e "${GREEN}=== Multi-Client Test ===${NC}\n"

# Check if server is running
if ! pgrep -f './server' > /dev/null; then
    echo -e "${YELLOW}Starting server...${NC}"
    ./server > server.log 2>&1 &
    sleep 1
fi

echo "This test demonstrates the key architectural improvements:"
echo ""
echo "1. Open TWO terminal windows"
echo "2. In terminal 1, run: ./build/new_client"
echo "3. In terminal 2, run: ./build/new_client"
echo ""
echo "Test scenarios:"
echo ""
echo "A. Room Creation Sync:"
echo "   - Terminal 1: Login as 'Alice'"
echo "   - Terminal 2: Login as 'Bob'"
echo "   - Alice: Create room 'TestRoom'"
echo "   → Bob should see 'TestRoom' appear in foyer immediately"
echo ""
echo "B. Chat Communication:"
echo "   - Both join 'TestRoom'"
echo "   - Alice sends: 'Hello from Alice'"
echo "   - Bob sends: 'Hello from Bob'"
echo "   → Both should see messages immediately"
echo ""
echo "C. Leave/Rejoin:"
echo "   - Alice types: /leave"
echo "   → Alice returns to foyer"
echo "   → Bob stays in room"
echo "   - Alice rejoins TestRoom"
echo "   → Both can chat again"
echo ""
echo "D. Clean Exit:"
echo "   - Press Ctrl+C in any client"
echo "   → Should exit immediately, no hanging"
echo ""
echo -e "${GREEN}Key observations:${NC}"
echo "✓ No blocking on room creation"
echo "✓ Foyer updates without timeout loops"
echo "✓ Chat messages arrive immediately"
echo "✓ No deadlocks on exit"
echo ""
