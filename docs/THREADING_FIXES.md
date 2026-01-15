# Threading and Deadlock Fixes

## Problem
When leaving/exiting the application, a deadlock occurred with error:
```
terminate called after throwing an instance of 'std::system_error'
what():  Resource deadlock avoided
Aborted (core dumped)
```

## Root Causes

### 1. **Multiple Thread Joins**
- `stop_receiving()` called `join()` on the receive thread
- Then `disconnect()` also tried to `join()` the same thread
- Attempting to join an already-joined thread causes a deadlock

### 2. **Callback Execution During Cleanup**
- The receive thread could call callbacks after cleanup started
- This caused events to be published during shutdown
- Led to race conditions and potential mutex deadlocks

### 3. **Improper Cleanup Order**
- UI cleanup happened while receive thread might still be calling event handlers
- Event handlers could be modifying UI state during cleanup

## Solutions Applied

### 1. **Fixed ServerConnection Thread Management** ([ServerConnection.cpp](src/ServerConnection.cpp))

**stop_receiving():**
- Now only sets `connected_ = false` and clears callbacks
- Does NOT join the thread (avoids double-join)
- Lets `disconnect()` handle the thread join

**disconnect():**
- Calls `shutdown()` on socket to unblock `recv()`
- Joins thread outside of connection check
- Safely closes socket after thread exits
- Prevents socket from being used after close

**receive_loop():**
- Checks both `message_callback_` AND `connected_` before calling callbacks
- Prevents callbacks during shutdown phase
- More defensive about callback validity

### 2. **Improved Client Cleanup Sequence** ([client.cpp](src/client.cpp))

Proper cleanup order:
```cpp
1. ui.stop()              // Stop UI event loop first
2. message_handler.stop_listening()  // Clear callbacks
3. sleep(200ms)           // Let threads exit gracefully
4. connection.disconnect() // Join threads and close socket
5. ui.cleanup()           // Clean up UI resources
```

This ensures:
- No new events are processed during shutdown
- Callbacks are cleared before threads join
- Threads have time to exit cleanly
- Resources are freed in safe order

## Technical Details

### Thread Safety Improvements

**Before:**
```cpp
void stop_receiving() {
    if (receive_thread_.joinable()) {
        receive_thread_.join();  // ❌ Could be called multiple times
    }
}
```

**After:**
```cpp
void stop_receiving() {
    connected_ = false;  // ✅ Signal thread to stop
    message_callback_ = nullptr;  // ✅ Prevent callbacks
    disconnect_callback_ = nullptr;
    // Let disconnect() do the join
}
```

### Callback Safety

**Before:**
```cpp
if (message_callback_) {
    message_callback_(msg);  // ❌ Could be null during cleanup
}
```

**After:**
```cpp
if (message_callback_ && connected_) {  // ✅ Double-check
    message_callback_(msg);
}
```

## Testing Recommendations

1. Test graceful exit with `/quit` command
2. Test leaving room with `/leave` command  
3. Test server disconnect during chat
4. Test rapid connect/disconnect cycles
5. Monitor for memory leaks with valgrind

## Related Files

- [ServerConnection.cpp](src/ServerConnection.cpp) - Thread and socket management
- [client.cpp](src/client.cpp) - Cleanup sequence
- [MessageHandler.cpp](src/MessageHandler.cpp) - Event callback handling
