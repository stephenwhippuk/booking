# Authentication Library

Independent authentication server library providing token-based authentication with user roles and JSON persistence.

## Features

- Token-based authentication with 60-minute expiration
- Role-based user system (multiple roles per user)
- User registration and management
- Token validation (with 30-second caching)
- Token revocation (logout)
- JSON-based user database persistence
- Per-message token validation
- Thread-safe operations

## Architecture

The auth library consists of:
- **AuthServer**: TCP server handling authentication requests (port 3001)
- **AuthManager**: Core authentication logic, token management, and caching
- **AuthToken**: Token structure containing username, display_name, roles, timestamps
- **FileUserRepository**: JSON-based persistent user storage
- **AuthClient**: Client library for connecting to auth server

## Data Format

### User Database (users.json)

```json
{
  "users": [
    {
      "username": "alice",
      "password_hash": "...",
      "display_name": "Alice",
      "roles": ["user"]
    },
    {
      "username": "john",
      "password_hash": "...",
      "display_name": "John Doe",
      "roles": ["user", "moderator"]
    }
  ]
}
```

### AuthToken Structure

```cpp
struct AuthToken {
  std::string token;                    // Unique token ID
  std::string username;                 // User login
  std::string display_name;             // Display name
  std::vector<std::string> roles;       // User roles (e.g., ["user", "admin"])
  std::chrono::system_clock::time_point issued_at;
  std::chrono::system_clock::time_point expires_at;
  
  bool is_valid() const {
    return std::chrono::system_clock::now() < expires_at;
  }
};
```

## Network Protocol

Messages are JSON objects with the following structure:

```json
{
  "header": {
    "timestamp": "2024-01-01T12:00:00Z",
    "token": "auth_token_or_empty"
  },
  "body": {
    "type": "AUTH|VALIDATE|GET_USER_INFO|etc",
    "data": { /* type-specific fields */ }
  }
}
```

### Message Types

**Authenticate (AUTH):**
```json
{
  "body": {
    "type": "AUTH",
    "data": {
      "username": "alice",
      "password": "secret123"
    }
  }
}
```

Response (success):
```json
{
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

**Validate Token (per-message validation):**
```json
{
  "header": {
    "token": "eyJhbGc..."
  },
  "body": {
    "type": "VALIDATE_TOKEN",
    "data": {}
  }
}
```

Response:
```json
{
  "body": {
    "type": "TOKEN_VALID",
    "data": {
      "username": "alice",
      "roles": ["user"],
      "expires_at": "2024-01-02T12:00:00Z"
    }
  }
}
```

**Get User Info:**
```json
{
  "header": {
    "token": "eyJhbGc..."
  },
  "body": {
    "type": "GET_USER_INFO",
    "data": {}
  }
}
```

Response:
```json
{
  "body": {
    "type": "USER_INFO",
    "data": {
      "username": "alice",
      "display_name": "Alice",
      "roles": ["user"]
    }
  }
}
```

## Usage

### Starting the Auth Server

```bash
./build/auth_server          # Default port 3001
./build/auth_server 4000     # Custom port
```

### Example Client Code

```cpp
#include "auth/AuthClient.h"
#include "auth/AuthToken.h"

// Create client connection
AuthClient auth_client("localhost", 3001);

// Authenticate
NetworkMessage auth_msg = NetworkMessage::create_auth("alice", "secret123");
auto response = auth_client.send_request(auth_msg);

if (response.body.type == "AUTH_SUCCESS") {
  std::string token = response.body.data["token"];
  std::string username = response.body.data["username"];
  auto roles = response.body.data["roles"];
  std::cout << "Logged in as " << username << " with roles: ";
  for (const auto& role : roles) {
    std::cout << role << " ";
  }
}

// Validate token on subsequent messages
NetworkMessage msg = NetworkMessage::create_join_room(token, "General");
auto result = auth_client.send_request(msg);

// Get user info
NetworkMessage info_msg = NetworkMessage::create_info(token);
auto user_info = auth_client.send_request(info_msg);
```

## Performance

- **Token Validation**: 30-second cache for repeated validations
- **Database**: JSON file format enables atomic writes
- **Concurrency**: Per-client token validation with thread-safe cache

## Testing

User database comes pre-populated with test accounts (see users.json).

## Integration

### CMakeLists.txt

```cmake
add_subdirectory(lib/auth)
target_link_libraries(your_target auth_lib)
```

### Include Headers

```cpp
#include <auth/AuthServer.h>
#include <auth/AuthManager.h>
#include <auth/AuthToken.h>
```

## Security Notes

⚠️ **Current Implementation**: This is a basic implementation for demonstration purposes.

For production use, consider:
- Using proper password hashing (bcrypt, argon2)
- HTTPS/TLS for encrypted communication
- Rate limiting for authentication attempts
- Secure token storage
- Database persistence for users and tokens
- Password complexity requirements
- Multi-factor authentication

## Token Expiration

Tokens expire after 60 minutes by default. Expired tokens are automatically cleaned up and validation will fail.

## Thread Safety

All operations in `AuthManager` are thread-safe using mutex locks.

## Testing

The library includes a test user:
- Username: `test`
- Password: `test123`

## Future Enhancements

- [ ] Database persistence
- [ ] bcrypt password hashing
- [ ] Refresh tokens
- [ ] Role-based access control
- [ ] Session management
- [ ] Audit logging
- [ ] Rate limiting
- [ ] Account lockout policies
