# Network Protocol Documentation

## Overview

The chat system uses a JSON-based network protocol built on the `NetworkMessage` abstraction layer. All messages follow a two-part structure: header (metadata) and body (payload).

## Message Structure

### Header
Contains protocol metadata:
```json
{
  "timestamp": "2024-01-01T12:00:00Z",
  "token": "eyJhbGc..." // Empty for responses
}
```

- **timestamp**: ISO 8601 formatted timestamp (server-side)
- **token**: Authentication token from client (empty in responses)

### Body
Contains message-specific data:
```json
{
  "type": "AUTH|JOIN_ROOM|CREATE_ROOM|...",
  "data": { /* type-specific fields */ }
}
```

- **type**: Message type identifier
- **data**: Type-specific payload (JSON object)

## Full Message Format

```json
{
  "header": {
    "timestamp": "2024-01-01T12:00:00Z",
    "token": "auth_token_or_empty"
  },
  "body": {
    "type": "MESSAGE_TYPE",
    "data": {}
  }
}
```

## Message Types

### Authentication Messages

#### AUTH (Client → Auth Server)
Authenticate a user with credentials.

```json
{
  "header": {
    "timestamp": "2024-01-01T12:00:00Z",
    "token": ""
  },
  "body": {
    "type": "AUTH",
    "data": {
      "username": "alice",
      "password": "secret123"
    }
  }
}
```

**Response (AUTH_SUCCESS):**
```json
{
  "header": {
    "timestamp": "2024-01-01T12:00:00Z",
    "token": ""
  },
  "body": {
    "type": "AUTH_SUCCESS",
    "data": {
      "token": "eyJhbGc...",
      "username": "alice",
      "display_name": "Alice",
      "roles": ["user"]
    }
  }
}
```

**Response (ERROR):**
```json
{
  "body": {
    "type": "ERROR",
    "data": {
      "message": "Invalid credentials"
    }
  }
}
```

### Room Management Messages

#### JOIN_ROOM (Client → Chat Server)
Join an existing chat room.

```json
{
  "header": {
    "timestamp": "2024-01-01T12:00:00Z",
    "token": "eyJhbGc..."
  },
  "body": {
    "type": "JOIN_ROOM",
    "data": {
      "room_name": "General"
    }
  }
}
```

**Response (ROOM_JOINED):**
```json
{
  "body": {
    "type": "ROOM_JOINED",
    "data": {
      "room_name": "General",
      "participants": ["alice", "bob", "john"],
      "message": "Joined room successfully"
    }
  }
}
```

#### CREATE_ROOM (Client → Chat Server)
Create a new chat room and join it.

```json
{
  "header": {
    "timestamp": "2024-01-01T12:00:00Z",
    "token": "eyJhbGc..."
  },
  "body": {
    "type": "CREATE_ROOM",
    "data": {
      "room_name": "Dev Discussion"
    }
  }
}
```

**Response (ROOM_JOINED):** Same as JOIN_ROOM

#### LEAVE (Client → Chat Server)
Leave the current chat room.

```json
{
  "header": {
    "timestamp": "2024-01-01T12:00:00Z",
    "token": "eyJhbGc..."
  },
  "body": {
    "type": "LEAVE",
    "data": {}
  }
}
```

**Response (LEFT_ROOM):**
```json
{
  "body": {
    "type": "LEFT_ROOM",
    "data": {
      "message": "Left room successfully"
    }
  }
}
```

### Listing Messages

#### ROOM_LIST (Chat Server → Client)
Broadcast of available rooms (sent when entering foyer or on refresh).

```json
{
  "body": {
    "type": "ROOM_LIST",
    "data": {
      "rooms": ["General", "Dev Discussion", "Random"]
    }
  }
}
```

#### PARTICIPANT_LIST (Chat Server → Client)
Broadcast of current room participants.

```json
{
  "body": {
    "type": "PARTICIPANT_LIST",
    "data": {
      "participants": ["alice", "bob", "john"]
    }
  }
}
```

### Chat Messages

#### CHAT_MESSAGE (Client → Chat Server)
Send a chat message in the current room.

```json
{
  "header": {
    "timestamp": "2024-01-01T12:00:00Z",
    "token": "eyJhbGc..."
  },
  "body": {
    "type": "CHAT_MESSAGE",
    "data": {
      "content": "Hello everyone!"
    }
  }
}
```

#### MESSAGE (Chat Server → All Clients in Room)
Broadcast chat message to room participants.

```json
{
  "body": {
    "type": "MESSAGE",
    "data": {
      "sender": "alice",
      "content": "Hello everyone!",
      "timestamp": "2024-01-01T12:00:05Z"
    }
  }
}
```

### Application Control Messages

#### QUIT (Client → Chat Server)
Disconnect client from server.

```json
{
  "header": {
    "timestamp": "2024-01-01T12:00:00Z",
    "token": "eyJhbGc..."
  },
  "body": {
    "type": "QUIT",
    "data": {}
  }
}
```

### Error Messages

#### ERROR (Server → Client)
Generic error response for failed operations.

```json
{
  "body": {
    "type": "ERROR",
    "data": {
      "message": "Room does not exist",
      "details": ""
    }
  }
}
```

## Message Flow Examples

### Login Flow
```
Client                          Auth Server
  │                                │
  ├─ AUTH (username, password) ──▶ │
  │                                │
  │ ◀─ AUTH_SUCCESS (token, roles) ┤
  │                                │
```

### Room Join Flow
```
Client                          Chat Server
  │                                │
  ├─ JOIN_ROOM (room_name) ──────▶ │
  │  (token in header)             │
  │                                │
  │ ◀─ ROOM_JOINED (participants) ┤
  │                                │
  │ ◀─ PARTICIPANT_LIST (members) ┤
  │                                │
```

### Chat Message Flow
```
Client A                        Chat Server                       Client B
  │                                │                                │
  ├─ CHAT_MESSAGE (content) ──────▶ │                                │
  │  (token in header)             │                                │
  │                                ├─ MESSAGE (sender, content) ──▶ │
  │                                │                                │
```

## Implementation Details

### Serialization
Messages are serialized to JSON strings with a newline terminator for framing:
```
{"header":{...},"body":{...}}\n
```

### Deserialization
Newline-delimited parsing allows reading complete messages from streams:
1. Read until newline
2. Parse JSON string
3. Extract header and body

### Factory Methods
The `NetworkMessage` class provides type-safe factory methods:

```cpp
// Client → Server messages
NetworkMessage::create_auth(username, password);
NetworkMessage::create_join_room(token, room_name);
NetworkMessage::create_create_room(token, room_name);
NetworkMessage::create_leave(token);
NetworkMessage::create_chat_message(token, content);
NetworkMessage::create_quit(token);

// Server → Client messages
NetworkMessage::create_error(message, details);
NetworkMessage::create_room_joined(room_name, participants);
NetworkMessage::create_room_list(rooms);
NetworkMessage::create_participant_list(participants);
NetworkMessage::create_broadcast_message(sender, content);
```

### Token Validation
- Tokens are included in the message header for all authenticated requests
- Server validates token before processing each message
- Per-message validation ensures token expiration is detected immediately
- 30-second cache prevents auth server overload on repeated validation

## Performance Considerations

- **Message Size**: Typical messages 50-500 bytes
- **Timestamp**: Generated server-side to ensure consistency
- **Token Length**: ~256 bytes per token
- **Queue Depth**: Messages can be buffered up to queue capacity (default: 1000)

## Error Handling

All error conditions return ERROR messages:
- Invalid token: "Token validation failed"
- Room not found: "Room does not exist"
- Invalid username: "User not found"
- Database errors: "Database error occurred"

## Future Extensions

The JSON-based protocol supports future enhancements:
- Message read receipts (`MESSAGE_READ` type)
- Typing indicators (`TYPING` type)
- User presence (`USER_STATUS` type)
- Encrypted messages (base64 in `data` field)
- Message history (`GET_HISTORY` type)
