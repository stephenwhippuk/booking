# Chat Application Specification

## Status: ✅ IMPLEMENTED

Multi-server chat application with token-based authentication, user roles, and JSON network protocol.

## Architecture Overview

The system consists of three independent servers:

1. **Auth Server (port 3001)**: Token generation, user management, role validation
2. **Chat Server (port 3000)**: Room management, message broadcasting
3. **Client (ncurses UI)**: Queue-based message handling with network, application, and UI threads

See [ARCHITECTURE_REDESIGN.md](ARCHITECTURE_REDESIGN.md) for complete design documentation.

---

## Features

### Authentication
- ✅ Token-based authentication (60-minute expiration)
- ✅ Per-message token validation
- ✅ 30-second token cache for performance
- ✅ Automatic logout on token expiration

### User Roles
- ✅ Multiple roles per user (e.g., ["user", "admin"])
- ✅ Role propagation in AuthToken
- ✅ Role-based access control ready
- ✅ Example roles: user, moderator, admin

### Chat Functionality
- ✅ Create and join chat rooms
- ✅ Real-time message broadcasting
- ✅ Member list tracking
- ✅ Leave room functionality
- ✅ Disconnect handling

### Network Protocol
- ✅ JSON-based message format
- ✅ Header: timestamp (ISO 8601) + token
- ✅ Body: message type + JSON data
- ✅ Type-safe factory methods
- ✅ Newline-delimited framing

### Data Persistence
- ✅ JSON user database (users.json)
- ✅ User fields: username, password_hash, display_name, roles
- ✅ Atomic writes
- ✅ Extensible schema

### Client UI
- ✅ Login screen (ncurses)
- ✅ Foyer (room list with navigation)
- ✅ Chat room (message history + member list)
- ✅ Queue-based architecture (non-blocking)
- ✅ Thread isolation (network, application, UI)

---

## Application Flow

### Client-Side Event System

The client uses a message-passing architecture with three independent threads:

```
Network Thread ←→ Application Thread ←→ UI Thread
   (TCP I/O)      (Business Logic)     (ncurses)
    (JSON)         (State Management)   (Rendering)
```

### Login Flow

1. **UI**: Login screen displayed, user enters credentials
2. **Input**: UI sends `LOGIN:username:password` event
3. **App**: Parse input, create `NetworkMessage::create_auth(username, password)`
4. **Network**: Send JSON to auth server (port 3001)
5. **Auth**: Validate credentials, return token + roles
6. **App**: Parse response, update state with username and token
7. **UI**: Display foyer screen
8. **Response**:
   - Success: Display foyer with available rooms and user's roles
   - Failure: Display error, return to login

### Foyer Flow

When **logged_in** event occurs:
1. **App**: Send message to chat server: `JOIN_ROOM:Foyer`
2. **Chat**: Return list of available rooms
3. **Network**: Receive JSON ROOM_LIST message
4. **App**: Parse room list, update state
5. **UI**: Render foyer with room navigation
6. **Commands**:
   - Join existing room: User selects room, sends `JOIN_ROOM:RoomName`
   - Create new room: User enters name, sends `CREATE_ROOM:RoomName`

### Room Selection

**Join Existing Room**:
1. **Input**: User presses Enter on selected room
2. **App**: Send `NetworkMessage::create_join_room(token, room_name)`
3. **Network**: Send JSON JOIN_ROOM to chat server
4. **Chat**: Validate token, add client to room, broadcast member list
5. **Response**: 
   - Success: Receive ROOM_JOINED + PARTICIPANT_LIST
   - Failure: Receive ERROR, stay in foyer

**Create New Room**:
1. **Input**: User enters room name in dialog
2. **App**: Send `NetworkMessage::create_create_room(token, room_name)`
3. **Network**: Send JSON CREATE_ROOM to chat server
4. **Chat**: Create room, add client, broadcast member list
5. **Response**: Same as join (success or error)

When room joined, **UI** displays:
- Chat message history area
- Member list (participants in room)
- Input field for messages
- Status bar with room name and user's roles

### Chat Room Commands

In chat room, user types message or command:

**Chat Message**:
- Type: `Hello everyone!`
- Action: Send `CHAT_MESSAGE:content` input event
- App: Create `NetworkMessage::create_chat_message(token, content)`
- Network: Send JSON to chat server
- Chat: Broadcast MESSAGE to all clients in room
- All UIs: Display message with sender and timestamp

**Commands**:
- `/leave`: Send LEAVE message, return to foyer
- `/quit`: Send QUIT message, close application
- Invalid: Display error message

### Server Push Messages

**PARTICIPANT_LIST (Room Updates)**:
- Sent when: User joins/leaves room
- Recipients: All clients in that room
- Action: UI updates member list display

**MESSAGE (Chat Broadcast)**:
- Sent when: Client sends CHAT_MESSAGE
- Recipients: All clients in that room (including sender)
- Content: sender name, message text, timestamp
- Action: Append to chat window

**ROOM_LIST (Room Updates)**:
- Sent when: Room created/destroyed
- Recipients: All clients in foyer
- Action: UI updates room list display

---

## Message Types

### Authentication
- **AUTH**: `{username, password}` → `{token, display_name, roles}`

### Room Management
- **JOIN_ROOM**: `{room_name}` → `{room_name, participants}`
- **CREATE_ROOM**: `{room_name}` → `{room_name, participants}`
- **LEAVE**: `{}` → `{message: "Left room"}`

### Information
- **ROOM_LIST**: Server → `{rooms: [...]}`
- **PARTICIPANT_LIST**: Server → `{participants: [...]}`

### Chat
- **CHAT_MESSAGE**: `{content}` → broadcast MESSAGE to room
- **MESSAGE**: Server → `{sender, content, timestamp}`

### System
- **QUIT**: `{}` → Close connection
- **ERROR**: Server → `{message, details}`

See [NETWORK_PROTOCOL.md](NETWORK_PROTOCOL.md) for complete JSON message format documentation.

---

## Data Format

### User Database (users.json)

```json
{
  "users": [
    {
      "username": "alice",
      "password_hash": "bcrypt_hash_here",
      "display_name": "Alice",
      "roles": ["user"]
    },
    {
      "username": "john",
      "password_hash": "bcrypt_hash_here",
      "display_name": "John Doe",
      "roles": ["user", "moderator"]
    },
    {
      "username": "stephen",
      "password_hash": "bcrypt_hash_here",
      "display_name": "Stephen",
      "roles": ["user", "admin"]
    }
  ]
}
```

### AuthToken Structure

```cpp
struct AuthToken {
  std::string token;                    // Unique ID
  std::string username;                 // Login name
  std::string display_name;             // Display name
  std::vector<std::string> roles;       // User roles
  std::chrono::system_clock::time_point issued_at;
  std::chrono::system_clock::time_point expires_at;  // 60 min
  
  bool is_valid() const;                // Check expiration
};
```

### Network Message

```json
{
  "header": {
    "timestamp": "2024-01-01T12:00:00Z",
    "token": "client_token_or_empty"
  },
  "body": {
    "type": "MESSAGE_TYPE",
    "data": {
      "field1": "value1",
      "field2": "value2"
    }
  }
}
```

---

## Implementation Status

### Completed ✅
- Three-server architecture (auth, chat, client)
- Token-based authentication system
- User roles with multi-role support
- JSON network protocol with factory methods
- JSON user database persistence
- Queue-based client architecture
- Full message type system (12 types)
- Per-message token validation
- ncurses UI (login, foyer, chat)
- Thread isolation (network, app, UI)

### Testing
- 40+ unit tests (ThreadSafeQueue, NetworkManager, ApplicationManager)
- Integration testing ready
- Manual testing with predefined users

### Documentation
- [ARCHITECTURE_REDESIGN.md](ARCHITECTURE_REDESIGN.md) - Complete system design
- [NETWORK_PROTOCOL.md](NETWORK_PROTOCOL.md) - Protocol specification
- [MIGRATION_COMPLETE.md](MIGRATION_COMPLETE.md) - Migration notes
- [lib/auth/README.md](../lib/auth/README.md) - Auth server documentation
- [lib/ui/README.md](../lib/ui/README.md) - UI component library

---

## Architecture Details

See [ARCHITECTURE.md](ARCHITECTURE.md) for full implementation details including:
- Event system design
- Message handler architecture  
- Command parser implementation
- UI decoupling strategy
- Component interaction patterns

See [SERVER_UPDATES.md](SERVER_UPDATES.md) for server-side changes including:
- Protocol message formats
- Room update notification system
- Broadcast message handling
- Real-time foyer updates
- Room update notification system
- Broadcast message handling
- Real-time foyer updates










