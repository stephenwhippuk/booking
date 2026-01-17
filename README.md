# Booking Chat System

A multi-threaded chat server and client application with distributed authentication, role-based access control, and JSON-based network protocol.

## Features

✅ **Three-Server Architecture**: Separate auth server, chat server, and client  
✅ **Token-Based Authentication**: 60-minute expiration with per-message validation  
✅ **User Roles**: Support for multiple roles per user (user, moderator, admin, etc.)  
✅ **JSON Database**: User data stored in JSON format with roles array  
✅ **JSON Network Protocol**: Type-safe messaging with NetworkMessage abstraction  
✅ **Queue-Based Architecture**: Pure message-passing with three independent threads  
✅ **Room Management**: Create rooms, join/leave, member tracking, broadcast messaging  
✅ **Component-Based UI**: Reusable ncurses widget library  
✅ **Thread Safety**: Lock-free queues, single-threaded state management  

## Screenshots

### Login Screen
![Login Screen](docs/images/login.png)

### Foyer with Room Selection
![Foyer](docs/images/foyer.png)

### Chatroom with Member List
![Chatroom](docs/images/chatroom.png)

## System Architecture

The application consists of three independent servers communicating via JSON over TCP:

```
┌─────────────────┐    tokens      ┌──────────────────┐    JSON msgs    ┌──────────────────────┐
│  Auth Server    │◀──register────▶│  Chat Server     │◀──────────────▶│  Client (ncurses UI) │
│  (port 3001)    │    validate    │  (port 3000)     │                │  (queue-based)       │
│  - Token mgmt   │    roles       │  - Room mgmt     │                │  - Network thread    │
│  - User DB      │                │  - Broadcasting  │                │  - App thread        │
│  - Role lookup  │                │  - Persistence   │                │  - UI thread         │
└─────────────────┘                └──────────────────┘                └──────────────────────┘
```

See [NETWORK_PROTOCOL.md](docs/NETWORK_PROTOCOL.md) for protocol details and [ARCHITECTURE_REDESIGN.md](docs/ARCHITECTURE_REDESIGN.md) for design documentation.

## Components

### Authentication Server (auth_server)
- **AuthManager**: Token generation and validation
- **AuthToken**: Token structure with username, roles, expiration
- **FileUserRepository**: JSON-based user database persistence
- **AuthServer**: TCP server on port 3001

### Chat Server (server)
- **ServerSocket**: TCP server management on port 3000
- **ClientManager**: Per-client session handling and protocol parsing
- **ChatRoom**: Room state, member tracking, message broadcasting
- **NetworkMessage**: JSON message protocol layer

### Client (client)
- **NetworkManager**: TCP transport layer with queue integration
- **ApplicationManager**: Protocol parsing and business logic
- **ApplicationState**: Single-threaded state management
- **UIManager**: ncurses presentation layer
- **UI Components**: Widget library (Window, TextInput, Menu, Label, ListBox, MessageBox)
- **ThreadSafeQueue**: Lock-free inter-thread communication

## Dependencies

- C++20 compiler (gcc 11+, clang 13+)
- CMake 3.20+
- ncurses development library
- nlohmann/json (automatically fetched via CMake)
- GoogleTest (automatically fetched for tests)

### Installing Dependencies

**Debian/Ubuntu:**
```bash
sudo apt-get install build-essential cmake libncurses-dev
```

**macOS:**
```bash
brew install cmake ncurses
```

## Building

```bash
mkdir -p build && cd build
cmake ..
make -j$(nproc)
```

Executables:
- `build/auth_server` - Authentication server (port 3001)
- `build/server` - Chat server (port 3000)
- `build/client` - Chat client (ncurses UI)
- `build/tests` - Unit tests (40+ tests)

## Running

### Terminal 1: Start Auth Server
```bash
./build/auth_server
# Output: Server listening on port 3001
```

### Terminal 2: Start Chat Server
```bash
./build/server
# Output: Server listening on port 3000
```

### Terminal 3 (and more): Start Client(s)
```bash
./build/client
```

### Test Users (predefined in users.json)
- **alice** / password `alice123` - Role: user
- **David** / password `david456` - Role: user
- **John** / password `john789` - Roles: user, moderator
- **Stephen** / password `stephen123` - Roles: user, admin

## Testing

### Unit Tests
```bash
./build/tests
```

Test coverage:
- ThreadSafeQueue: 15 tests
- NetworkManager: 8 tests
- ApplicationManager: 12 tests

### Manual Testing
```bash
# Single client
./test_new_client.sh

# Multi-client (open 2 terminals)
./test_multi_client.sh
```

## Client Usage

### Login Screen
- Type your name and press Enter
- Press 'q' to quit

### Foyer (Room List)
- Use ↑/↓ to navigate rooms
- Press Enter to join selected room
- Press 'c' to create a new room (opens dialog)
- Press 'q' to quit

### Chatroom
- Type message and press Enter to send
- Member list displayed on the right side
- `/leave` - Return to foyer
- `/quit` - Exit application

## Key Features

✅ **Non-blocking Architecture**: No thread ever blocks on another  
✅ **Zero Deadlocks**: Clean shutdown with Ctrl+C  
✅ **Immediate Updates**: No polling loops, instant room list updates  
✅ **Thread Safety**: Lock-free queues, single-threaded state  
✅ **Complete Separation**: Network, logic, and UI are independent  
✅ **Component-Based UI**: Reusable widget library for ncurses  
✅ **Member List**: Real-time display of room participants  
✅ **Room Creation**: Interactive dialog for creating new rooms  
✅ **Flicker-Free Rendering**: Double buffering for smooth UI updates

## Project Structure

```
.
├── src/
│   ├── client/
│   │   ├── client.cpp                 # Main client entry
│   │   ├── NetworkManager.*           # TCP transport
│   │   ├── ApplicationManager.*       # Business logic
│   │   ├── ApplicationState.*         # State mgmt
│   │   ├── UIManager.*                # UI rendering
│   │   └── ThreadSafeQueue.h          # Lock-free queue
│   ├── server/
│   │   ├── server.cpp                 # Main server entry
│   │   ├── ChatRoom.*                 # Room management
│   │   ├── ClientManager.*            # Client handling
│   │   └── ServerSocket.*             # TCP server
│   ├── auth_server.cpp                # Auth server main
│   └── RoomInfo.h                     # Shared data
├── lib/
│   ├── auth/
│   │   ├── include/auth/
│   │   │   ├── AuthManager.h          # Token mgmt
│   │   │   ├── AuthToken.h            # Token struct (roles field)
│   │   │   ├── AuthClient.h           # Client library
│   │   │   └── AuthServer.h           # Server impl
│   │   └── src/
│   │       ├── AuthManager.cpp
│   │       ├── AuthToken.cpp
│   │       ├── FileUserRepository.cpp # JSON storage
│   │       ├── AuthServer.cpp
│   │       └── AuthClient.cpp
│   ├── common/
│   │   └── include/common/
│   │       └── NetworkMessage.h       # JSON protocol layer
│   └── ui/
│       ├── include/ui/
│       │   ├── Widget.h               # Base widget
│       │   ├── Window.h               # Container
│       │   ├── TextInput.h            # Input field
│       │   ├── Menu.h                 # Selection list
│       │   ├── Label.h                # Static text
│       │   └── ListBox.h              # Read-only list
│       └── src/
│           └── [implementations]
├── tests/
│   ├── ThreadSafeQueueTest.cpp
│   ├── NetworkManagerTest.cpp
│   └── ApplicationManagerTest.cpp
├── docs/
│   ├── images/
│   ├── ARCHITECTURE_REDESIGN.md       # Full design doc
│   ├── NETWORK_PROTOCOL.md            # Protocol spec
│   ├── MIGRATION_COMPLETE.md          # Migration notes
│   ├── THREADING_FIXES.md             # Threading details
│   ├── spec.md                        # Feature spec
│   └── SERVER_UPDATES.md              # Server changes
├── users.json                          # User database (JSON)
├── CMakeLists.txt                      # Build config
└── README.md
```

## Development

### Adding Features

**New UI Command:**
1. Add to `UICommandType` enum in `UICommand.h`
2. Add handler in `UIManager::process_commands()`
3. Generate command in `ApplicationManager`

**New Protocol Message:**
1. Parse in `ApplicationManager::process_network_message()`
2. Update `ApplicationState`
3. Generate appropriate UI commands

**New Network Transport:**
1. Implement interface matching `NetworkManager`
2. Use same queue references
3. No changes needed to Application or UI layers

### Testing

All components are unit tested in isolation:
- Create mock queues
- Push test data
- Verify output

See `tests/` directory for examples.

## Documentation

- [ARCHITECTURE_REDESIGN.md](docs/ARCHITECTURE_REDESIGN.md) - Complete design document
- [MIGRATION_COMPLETE.md](docs/MIGRATION_COMPLETE.md) - Migration summary and comparison
- [UI Component Library](lib/ui/README.md) - ncurses widget documentation
- [spec.md](docs/spec.md) - Original specification

## License

MIT

## Archive

Legacy event-based implementation is preserved in `archive/old_architecture/` for reference.
