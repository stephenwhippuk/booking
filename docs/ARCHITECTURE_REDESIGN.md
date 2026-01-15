# Architecture Redesign: Thread-Safe Message-Passing System

## Current Problems

### Synchronous Cross-Thread Calls
- Network receive thread directly invokes UI functions via event handlers
- Event handlers execute on the caller's thread (receive thread)
- UI operations block network reception (e.g., `show_foyer_screen()`, `run_input_loop()`)

### Blocking Issues Observed
1. **Room creation hangs**: Creating room from foyer blocks waiting for server response
2. **Receive thread blocking**: `recv()` blocks during UI operations
3. **Race conditions**: Multiple threads modifying UI state (`current_state_`, `current_rooms_`)
4. **Deadlocks**: Thread cleanup during shutdown

### Root Cause
**Direct coupling between components across thread boundaries** - the event bus provides decoupling at compile-time but NOT at runtime (events execute handlers synchronously on calling thread).

---

## Proposed Architecture

### Core Principle
**Complete separation of concerns with dedicated queues for each layer**

### Threading Model

```
┌──────────────────┐    Network Protocol    ┌──────────────────────┐    UI Commands    ┌─────────────────┐
│  Network Thread  │      (strings)         │  Application Thread  │    (commands)     │   UI Thread     │
│                  │                        │   (Event Bus)        │                   │    (Main)       │
│  - recv() loop   │───[inbound queue]────▶│  - Process network   │──[UI queue]─────▶│  - Poll UI      │
│  - send() loop   │◀──[outbound queue]────│  - Event dispatch    │                   │    queue        │
│  - TCP socket    │                        │  - Business logic    │                   │  - Render       │
│                  │                        │  - State management  │                   │  - Input poll   │
└──────────────────┘                        └──────────────────────┘                   └─────────────────┘
                                                       │                                         │
                                                       ▼                                         │
                                             ┌──────────────────┐                               │
                                             │  Application     │                               │
                                             │     State        │◀──────────────────────────────┘
                                             │  (Read-only UI)  │      (input events)
                                             └──────────────────┘
```

### Queue Architecture

**Network Layer Queues**:
- `network_inbound`: Raw server messages (strings) → consumed by Application thread
- `network_outbound`: Raw client messages (strings) → consumed by Network thread

**UI Layer Queue**:
- `ui_command_queue`: UI update commands → consumed by UI thread

**Benefits of Separated Queues**:
- Network thread knows nothing about events or UI
- Application thread is pure business logic
- UI thread knows nothing about network protocol
- Easy to swap implementations (e.g., different protocol, different UI)

---

## Component Details

### 1. Application Thread (Event Bus + Business Logic)

**Purpose**: Process network messages, dispatch events, manage state, generate UI commands

**Responsibilities**:
1. **Consume network inbound queue**: Parse server messages into events
2. **Event dispatch**: Invoke event handlers for business logic
3. **State management**: Update ApplicationState based on events
4. **UI command generation**: Translate state changes into UI commands
5. **Network command generation**: Enqueue outbound network messages

**Interface**:
```cpp
class ApplicationManager {
private:
    // Input queues
    ThreadSafeQueue<std::string> network_inbound_;
    
    // Output queues
    ThreadSafeQueue<std::string> network_outbound_;
    ThreadSafeQueue<UICommand> ui_commands_;
    
    // Internal processing
    EventBus event_bus_;
    ApplicationState app_state_;
    std::thread app_thread_;
    bool running_;

public:
    void start();  // Start application thread
    void stop();   // Stop application thread
    
    // Queue accessors
    ThreadSafeQueue<std::string>& get_network_inbound() { return network_inbound_; }
    ThreadSafeQueue<std::string>& get_network_outbound() { return network_outbound_; }
    ThreadSafeQueue<UICommand>& get_ui_commands() { return ui_commands_; }
    
private:
    void application_loop();  // Main processing loop
    void process_network_message(const std::string& msg);
    void handle_event(const Event& event);
    void send_ui_command(const UICommand& cmd);
};
```

**Processing Loop**:
```cpp
void ApplicationManager::application_loop() {
    while (running_) {
        // 1. Process network messages
        std::string msg;
        if (network_inbound_.try_pop(msg, 10ms)) {
            process_network_message(msg);
        }
        
        // 2. Dispatch internal events
        Event event;
        if (event_bus_.try_pop_event(event, 10ms)) {
            handle_event(event);
        }
    }
}
```

---

### 2. Network Thread

**Purpose**: Pure TCP I/O - receive from socket and send from queue

**Responsibilities**:
1. **Receive loop**: `recv()` → enqueue to network_inbound
2. **Send loop**: Drain network_outbound → `send()` to socket
3. **No parsing**: Just byte streams, completely protocol-agnostic
4. **No business logic**: Doesn't know about events, state, or UI

**Interface**:
```cpp
class NetworkManager {
private:
    ThreadSafeQueue<std::string>& inbound_queue_;   // Reference to app's queue
    ThreadSafeQueue<std::string>& outbound_queue_;  // Reference to app's queue
    std::thread network_thread_;
    int socket_;
    bool running_;

public:
    NetworkManager(ThreadSafeQueue<std::string>& inbound,
                   ThreadSafeQueue<std::string>& outbound);
    
    void start();   // Start network thread
    void stop();    // Stop network thread
    bool connect(const std::string& host, int port);
    void disconnect();
    
private:
    void network_loop();  // Runs on network thread
    void receive_data();  // recv() and enqueue
    void send_data();     // Dequeue and send()
};
```

**Network Loop**:
```cpp
void NetworkManager::network_loop() {
    while (running_) {
        // NoPoll UI command queue, render screens, handle input

**Responsibilities**:
1. **Poll UI command queue**: Check for render commands from Application thread
2. **Render screens**: Update ncurses based on UI commands
3. **Handle input**: Non-blocking input check, send input events back to Application
4. **No business logic**: Purely presentation layer

**UI Commands**:
```cpp
enum class UICommandType {
    SHOW_LOGIN,
    SHOW_FOYER,
    SHOW_CHATROOM,
    UPDATE_ROOM_LIST,
    ADD_CHAT_MESSAGE,
    SHOW_ERROR,
    CLEAR_INPUT,
    QUIT
};

struct UICommand {
    UICommandType type;
    std::any data;  // Command-specific payload
};
```

**Interface**:
```cpp
class UIManager {
private:
    ThreadSafeQueue<UICommand>& ui_commands_;
    ThreadSafeQueue<std::string>& input_events_;  // Send to Application
    
    // UI state (local to UI thread)
    UIScreen current_screen_;
    std::vector<RoomInfo> rooms_;
    std::vector<std::string> chat_messages_;
    std::string input_buffer_;
    
    bool running_;

public:
    UIManager(ThreadSafeQueue<UICommand>& commands,
              ThreadSafeQueue<std::string>& input);
    
    void run();  // Main UI loop - owned by Application thread

**Key Properties**:
```cpp
class ApplicationState {
private:
    // No mutex needed - only accessed by Application thread!
    
    // Connection state
    bool connected_;
    std::string username_;
    
    // Current screen
    enum class Screen { LOGIN, FOYER, CHATROOM };
    Screen current_screen_;
    
    // Foyer state
    std::vector<RoomInfo> rooms_;
    
    // Chatroom state
    std::string current_room_;
    std::vector<std::string> chat_lines_;
    
public:
    // Simple getters/setters (no locking)
    void set_screen(Screen screen);
    Screen get_screen() const;
    
    void set_rooms(const std::vector<RoomInfo>& rooms);
    std::vector<RoomInfo> get_rooms() const;
    
    void add_chat_line(const std::string& line);
    std::vector<std::string> get_chat_lines() const;
    
    // ... etc for all state
};
```

**Thread Safety Note**:
- State is ONLY accessed by Application thread
- Other threads communicate via queues
- No mutex needed - single-threaded access
- UI gets updates via commands, not by reading state directly             break;
            // ... etc
        }
    }
}input_events_.push("ROOM_SELECTED:General")

2. Application Thread: Process input event
   └─> Parse "ROOM_SELECTED:General"
   └─> network_outbound_.push("JOIN_ROOM:General\n")

3. Network Thread: Drain outbound queue
   └─> send() "JOIN_ROOM:General\n" to server

4. Network Thread: recv() returns "JOINED_ROOM:General\n"
   └─> network_inbound_.push("JOINED_ROOM:General\n")

5. Application Thread: Process network message
   └─> Parse "JOINED_ROOM:General"
   └─> app_state_.set_screen(CHATROOM)
   └─> app_state_.set_current_room("General")
   └─> ui_commands_.push(UICommand{SHOW_CHATROOM})

6. UI Thread: Process UI command
   └─> current_screen_ = CHATROOM
   └─> render_chatroom()
```

**Key Points**:
- Each thread only touches its own queues
- Application thread is the only one processing logic
- Network thread is pure I/O
- UI thread is pure presentation
- Clean separation of concern
private:
    void update();     // Check state, render appropriate screen
    void poll_input(); // Non-blocking input check
    void render_login();
    voiinput_events_.push("CREATE_ROOM:MyRoom")

2. Application Thread: Process input
   └─> network_outbound_.push("CREATE_ROOM:MyRoom\n")

3. Network Thread: Send to server
   └─> send() to server

4. Network Thread: Receive response
   └─> recv() returns "JOINED_ROOM:MyRoom\n"
   └─> network_inbound_.push("JOINED_ROOM:MyRoom\n")

5. Application Thread: Process response
   └─> app_state_.set_screen(CHATROOM)
   └─> ui_commands_.push(UICommand{SHOW_CHATROOM})

6. UI Thread: Update display
   └─> current_screen_ = CHATROOM
```

**No More Blocking**: UI continues rendering foyer while waiting for response
---

### 4. Application State

**Purpose**: Single source of truth for app state, thread-safe access

**Key Properties**:
```cpp
class ApplicationState {
private:
    mutable std::mutex mutex_;
    
    // Connection state
    bool connected_;
    std::string username_;
    
    // UI state
    enum class Screen { LOGIN, FOYER, CHATROOM };
    Screen current_screen_;
    
    // Foyer state
    std::vector<RoomInfo> rooms_;
    
    // Chatroom state
    std::string current_room_;
    std::vector<std::string> chat_lines_;
    
public:
    // Thread-safe getters/setters
    void set_screen(Screen screen);
    Screen get_screen() const;
    network_inbound_.push("ROOM_LIST\n...")

2. Application Thread: Process room list
   └─> Parse rooms
   └─> app_state_.set_rooms([...])
   └─> ui_commands_.push(UICommand{UPDATE_ROOM_LIST, rooms})

3. UI Thread: Update display
   └─> rooms_ = [...]
   └─> render_foyer() shows new list
```

**Automatic Refresh**: UI updates immediately when command arrives
---

## Event Flow Examples

### Example 1: User Joins Room

```
1. UI Thread: User presses Enter on room
   └─> publish(ROOM_SELECTED, room_name="General")

2. Event Bus Thread: Dispatch ROOM_SELECTED
   └─> MessageHandler::handle_room_selected()
       └─> network.send_message("JOIN_ROOM:General\n")  [non-blocking enqueue]

3. Network Thread: Drain outbound queue
   └─> send() to server
current_screen_) {
        case Screen::LOGIN:
            handle_login_input(ch);
            break;
        case Screen::FOYER:
            handle_foyer_input(ch);
            break;
        case Screen::CHATROOM:
            handle_chatroom_input(ch);
            break;
    }
}

void UIManager::handle_foyer_input(int ch) {
    if (ch == KEY_UP) {
        // Adjust local selection state (UI-only)
        selected_index_ = std::max(0, selected_index_ - 1);
    } else if (ch == '\n') {
        // Send input event to Application
        std::string room_name = rooms_[selected_index_].name;
        input_events_.push("ROOM_SELECTED:" + room_name);
    } else if (ch == 'q') {
        input_events_.push("QUIT");
    }
}
```

**Benefits**:
- Single input polling point
- Screen-specific logic clearly separated
- No blocking - just check, process, return
- UI-only state (selection) stays in UI thread
- Application state changes via input events
   └─> recv() returns "JOINED_ROOM:MyRoom\n"
   └─> publish(ROOM_JOINED, room_name="MyRoom")

4. Event Bus Queue Infrastructure
1. Create `ThreadSafeQueue<T>` template class
2. Create queue types: `network_inbound`, `network_outbound`, `ui_commands`, `input_events`
3. Test queue operations in isolation
4. **Verify**: Queues are thread-safe and performant

### Phase 2: Network Layer Separation
1. Create `NetworkManager` with queue references
2. Replace `ServerConnection` send/receive with queue operations
3. Network thread only does I/O, enqueues raw strings
4. **Verify**: Network messages flow through queues correctly

### Phase 3: Application Thread (Business Logic)
1. Create `ApplicationManager` with processing loop
2. Move message parsing from `MessageHandler` to Application thread
3. Move event handlers to Application thread
4. Generate outbound network messages and UI commands
5. **Verify**: Application logic processes messages correctly

### Phase 4: UI Command System
1. Define `UICommand` types and structures
2. Create `UIManager` that polls `ui_commands_` queue
3. Replace state reads with local UI state updated by commands
4. **Verify**: UI renders based on commands

### Phase 5: Input Event System
1. Replace event publishing with input event strings
2. UI sends input events via `input_events_` queue
3. Application thread processes input events
4. **Verify**: Input flows UI → App → Network correctly

### Phase 6: Cleanup
1. Remove old `EventBus` pub/sub if not needed internally
2. Remove `ChatUI` blocking loops
3. Remove `MessageHandler` direct UI calls
4. **Verify**: Clean separation, no cross-thread calls

### Phase 7: Testing & Polish
1. Test all user flows (login, join, leave, quit, create room, chat)
2. Verify thread cleanup on exit
3. Add error handling for queue failures
4. Performance tuning (queue sizes, poll intervals)
5. **Verify**: Production-ready systemith large timeout)
- Foyer and chatroom have separate input loops
- Input handling mixed with rendering

### Proposed Solution

**Unified Input Polling**:
```cpp
void UIManager::poll_input() {
    int ch = getch();  // With nodelay() enabled
    if (ch == ERR) return;  // No input available
    
    switch (app_state_.get_screen()) {
        case Screen::LOGIN:
            handle_login_input(ch);
            break;
        case Screen::FOYER:
            handle_foyer_input(ch);
            break;
        case Screen::CHATROOM:
            handle_chatroom_input(ch);
            break;
    }
}

void UIManager::handle_foyer_input(int ch) {
    if (ch == KEY_UP) {
        // Adjust local selection state
    } else if (ch == '\n') {
        // User selected room
        std::string room_name = rooms_[selected_index_].name;
        event_bus_.publish(Event(ROOM_SELECTED, room_name));
    } else if (ch == 'q') {
        event_bus_.publish(Event(APP_KILLED));
    }
}
```

**Benefits**:
- Single input polling point
- Screen-specific logic clearly separated
- No blocking - just check, process, return
- State machine approach (current screen determines input interpretation)

---

## Migration Strategy

### Phase 1: Event Bus Queue (Foundation)
1. Add event queue to EventBus
2. Add dispatch thread
3. Modify publish() to enqueue
4. Test with existing handlers (should work identically)
5. **Verify**: No behavioral changes, just internal queueing

### Phase 2: Network Thread Separation
1. Create NetworkManager with outbound queue
2. Move receive loop to network thread
3. Replace `connection_.send_message()` calls with queue enqueues
4. Update message parsing to publish events (not call handlers)
5.ThreadSafeQueue Template**:
```cpp
template<typename T>
class ThreadSafeQueue {
private:
    std::queue<T> queue_;
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    bool stopped_ = false;

public:
    void push(const T& item) {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.push(item);
        cv_.notify_one();
    }
    
    bool try_pop(T& item, std::chrono::milliseconds timeout) {
        std::unique_lock<std::mutex> lock(mutex_);
        
        if (!cv_.wait_for(lock, timeout, [this]{ 
            return !queue_.empty() || stopped_; 
        })) {
            return false;  // Timeout
        }
        
        if (stopped_ && queue_.empty()) {
            return false;  // Stopped
        }
        
        item = queue_.front();
        queue_.pop();
        return true;
    }
    
    void stop() {
        std::lock_guard<std::mutex> lock(mutex_);
        stopped_ = true;
        cv_.notify_all();
    }
};
```

**Application State**:
- NO mutex needed - only accessed by Application thread
- Single-threaded access eliminates race conditions
- UI gets copies via commands, never reads state directly

**Queue Communication Pattern**:
- Producer: `queue.push(item)` - never blocks (except on mutex)
- Consumer: `queue.try_pop(item, timeout)` - blocks for timeout or until item available
- All threads can cleanly shutdown via `queue.stop()`
    while (running_) {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        cv_.wait(lock, [this]{ return !event_queue_.empty() || !running_; });
        
        if (!running_) break;
        
        Event event = event_queue_.front();
        event_queue_.pop();
        lock.unlock();  // Release lock before invoking handlers
        
        // Invoke handlers (on event bus thread)
        auto& handlers = subscribers_[event.type];
        for (auto& handler : handlers) {
            handler(event);
        }commands arrive

### Reliability
- **No deadlocks**: Threads communicate only via queues
- **Clean shutdown**: Each queue can be stopped independently
- **Predictable behavior**: Clear unidirectional data flow

### Maintainability
- **Clear responsibilities**: Each thread has single, well-defined purpose
- **Easy debugging**: Can log all queue operations
- **Testable**: Can mock queues, test components in isolation
- **Complete decoupling**: Network, Application, UI know nothing about each other

### Scalability
- **Easy to add features**: Add new command types, extend queues
- **Independent evolution**: Replace network protocol without touching UI
- **Replace UI**: Could add web UI, GUI, CLI alongside ncurses
- **Performance tuning**: Can optimize each thread independently

### Architecture Quality
- **Separation of concerns**: Network (I/O), Application (logic), UI (presentation)
- **SQueue Sizes**: Should queues have max capacity? What happens if queue fills up?

2. **Command Batching**: Should we send UI commands in batches or one at a time?

3. **Input Event Format**: String-based (easy to serialize) vs. structured (type-safe)?

4. **Priority Queues**: Do some commands need priority (e.g., QUIT)?

5. **Message History**: Should we log all queue traffic for debugging/replay?

6. **Back-pressure**: What if UI commands are generated faster than rendering can handle?

7. **State Queries**: Does UI ever need to query Application state, or only receive commands
- Clean thread shutdown

**Server Errors**:
- Parse error responses (ROOM_EXISTS, ROOM_NOT_FOUND)
- Publish error events
- Event handlers update app state with error message
- UI displays error in current context

---

## Expected Benefits

### Performance
- **Non-blocking operations**: UI never freezes
- **Responsive input**: Input processed every 50ms regardless of network
- **Efficient rendering**: Only redraw when state changes (can add dirty flag)

### Reliability
- **No deadlocks**: Threads don't block each other
- **Clean shutdown**: Each thread has clear shutdown path
- **Predictable behavior**: State changes follow clear event flow

### Maintainability
- **Clear responsibilities**: Each component has single purpose
- **Easy debugging**: Event flow can be logged/traced
- **Testable**: Components can be tested in isolation

### Scalability
- **Easy to add features**: New events and handlers
- **Independent evolution**: Change UI without touching network code
- **Performance tuning**: Can optimize each thread independently

---

## Open Questions

1. **Event Handler Duration**: What if an event handler takes too long? Should we add timeout/watchdog?

2. **State Snapshot vs. Live Read**: Should UI take a snapshot of state at start of frame, or read live?

3. **Priority Events**: Do some events need priority (e.g., APP_KILLED)?

4. **Event History**: Should we log events for debugging/replay?

5. **Back-pressure**: What if events are generated faster than dispatch can handle?

---

## Next Steps

1. **Review this design** - feedback and adjustments
2. **Phase 1 implementation** - Event Bus queue infrastructure
3. **Integration testing** - Verify no regressions
4. **Incremental migration** - Phases 2-5 with testing between each
5. **Performance validation** - Measure responsiveness improvements

---

## Conclusion

This redesign addresses the fundamental coupling issues by introducing proper **thread isolation** and **message-passing**. While it requires significant refactoring, the result will be:

- **More responsive** - No blocking operations
- **More reliable** - No deadlocks or race conditions  
- **More maintainable** - Clear component boundaries
- **More correct** - Proper thread safety

The migration can be done incrementally with testing at each phase, minimizing risk.
