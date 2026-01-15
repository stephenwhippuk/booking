#!/bin/bash
# Test script for new queue-based architecture client

set -e

BUILD_DIR="$(cd "$(dirname "$0")/build" && pwd)"
cd "$BUILD_DIR"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${GREEN}=== Testing New Queue-Based Architecture ===${NC}\n"

# Check if server is running
if pgrep -f './server' > /dev/null; then
    echo -e "${GREEN}✓${NC} Server is running"
else
    echo -e "${YELLOW}Starting server...${NC}"
    ./server > server.log 2>&1 &
    SERVER_PID=$!
    sleep 1
    
    if pgrep -f './server' > /dev/null; then
        echo -e "${GREEN}✓${NC} Server started (PID: $SERVER_PID)"
    else
        echo -e "${RED}✗${NC} Failed to start server"
        exit 1
    fi
fi

echo -e "\n${GREEN}=== Test Instructions ===${NC}"
echo "1. Login Screen:"
echo "   - Type your name and press Enter"
echo "   - Press 'q' to quit"
echo ""
echo "2. Foyer Screen:"
echo "   - Use ↑/↓ to navigate rooms"
echo "   - Press Enter to join selected room"
echo "   - Type room name and press Enter to create new room"
echo "   - Press 'q' to quit"
echo ""
echo "3. Chatroom:"
echo "   - Type messages and press Enter to send"
echo "   - Type '/leave' to return to foyer"
echo "   - Type '/quit' to exit"
echo ""
echo -e "${YELLOW}=== Key Architecture Features to Observe ===${NC}"
echo "✓ No blocking - UI should always be responsive"
echo "✓ No deadlocks - clean exit with Ctrl+C"
echo "✓ Room list updates immediately when others create rooms"
echo "✓ Chat messages appear smoothly"
echo ""
echo -e "${GREEN}Press Enter to launch client...${NC}"
read

# Launch the new client
./new_client

# Cleanup
echo -e "\n${GREEN}Client exited cleanly${NC}"

# Offer to show logs
echo ""
echo -e "Server log available at: ${YELLOW}$BUILD_DIR/server.log${NC}"
echo "View server log? (y/N)"
read -n 1 -r
echo
if [[ $REPLY =~ ^[Yy]$ ]]; then
    tail -20 server.log
fi
