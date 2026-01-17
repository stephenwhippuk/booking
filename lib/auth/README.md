# Authentication Library

Independent authentication server library providing token-based authentication.

## Features

- Token-based authentication
- User registration
- Token validation
- Token revocation (logout)
- Automatic token expiration
- Thread-safe operations

## Architecture

The auth library consists of:
- **AuthServer**: TCP server handling authentication requests
- **AuthManager**: Core authentication logic and token management
- **AuthToken**: Token data structure with expiration

## Protocol

The auth server uses a simple text-based protocol over TCP:

### Commands

**Authenticate User:**
```
Request:  AUTH username password
Response: OK <token>          (success)
          FAILED              (invalid credentials)
```

**Validate Token:**
```
Request:  VALIDATE <token>
Response: VALID               (token is valid)
          INVALID             (token expired or invalid)
```

**Get Username from Token:**
```
Request:  GETUSER <token>
Response: USER <username>     (token valid)
          NOTFOUND            (token invalid)
```

**Register New User:**
```
Request:  REGISTER username password
Response: REGISTERED          (success)
          EXISTS              (username taken)
```

**Revoke Token (Logout):**
```
Request:  REVOKE <token>
Response: REVOKED
```

## Usage

### Starting the Auth Server

```bash
./build/auth_server          # Default port 3001
./build/auth_server 4000     # Custom port
```

### Example Client Code

```cpp
#include "auth/AuthManager.h"

AuthManager auth;

// Register user
auth.register_user("alice", "secret123");

// Authenticate
AuthToken token = auth.authenticate("alice", "secret123");
if (token.is_valid) {
    std::cout << "Token: " << token.token << "\n";
}

// Validate token
if (auth.validate_token(token.token)) {
    std::cout << "Token is valid\n";
}

// Get username
auto username = auth.get_username(token.token);
if (username) {
    std::cout << "User: " << *username << "\n";
}

// Revoke token
auth.revoke_token(token.token);
```

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
