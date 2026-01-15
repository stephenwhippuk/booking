# Chat Application Event-Driven Architecture Specification

## Status: ✅ IMPLEMENTED

This specification describes an event-driven architecture that completely decouples UI, network, and business logic.

## Client-Side Event System

There is a client-side event system which is solely responsible for controlling changes between UI pages.

## Server Messaging System

There is a server messaging system that is 2-way. Events can raise messages to the server. The processing of server messages on the client can raise events.

## Application Flow

### Initialization

When the app starts, it should do its normal initialisation stuff then when this is complete, then an **initialized** event is created.

When an **initialized** event is received it should open the login screen.

### Login Flow

When the login screen is displayed, `<enter>` should raise a **login_submitted** event.

A **login_submitted** event should lead to a `login_attempt` message being sent to the server. The message will return a result object containing a status and a message.

- If failure then the message should be displayed and user can alter and submit again
- If success then a **logged_in** event occurs

### Foyer Flow

When a **logged_in** event occurs then we need to join the foyer. So a `join_foyer` request is sent to server. This will return the current foyer object containing the available rooms and status. The **logged_in event** should also cause a loading screen to be displayed.

- If failed then this indicates something wrong with the login on the server therefore a **kicked** event should be raised
- If successful then local room data should be updated and **foyer_joined** event raised

When **kicked** event is raised the UI should return to the login page.

When a **foyer_joined** event is raised, then the foyer page should be displayed.

### Room Selection

If an existing room is selected then a **room_selected** event is raised.

If a new room is requested then a **room_requested** event is raised.

When a **room_selected** event is raised it should send a `join_room` message to the server.

When a **room_requested** event is raised it should send a `create_and_join_room` message to the server.

Both of which will return a status and room object, which will hold a list of participants as well as the chat history. As with the joining the foyer a loading screen should be displayed.

- If success then a **room_joined** event is raised
- If failed then a **foyer_joined** event is raised

When a **room_joined** event is raised the chatroom page should be displayed.

### Chat Room Commands

The chatroom has a command and chat window. Pressing `<enter>` will cause a **command_submitted** event to be raised with the text. This will then be sent to a command parser, which will do the following:

- If `/quit` then a **app_killed** event is raised
- If `/logout` then a **logout_requested** event is raised
- If `/leave` then a **leave_requested** event is raised
- If `/<anything else>` then a **command_not_recognised** event is raised
- Otherwise it is a chat message and a **chat_line_submitted** event is raised

#### Command Handling

If a **app_killed** is raised then the application should safely exit.

If a **logout_requested** event is raised then a `logout_request` message should be sent to the server. This should return a status code:
- If successful then a **logged_out** event should be raised
- Otherwise a **kicked** event should be raised

If a **leave_requested** event is raised then a `leave_room` message should be sent to the server. This can be fire and forget. It will then raise a **logged_in** event to bring the foyer back up.

Otherwise a `chatline` message is produced.

### Server Push Messages

In all of this there can be server push messages, for instance when a chat message is broadcast etc.

When a client sends a message a **chat_receipt** message will be broadcast to all clients. This will cause a **chat_received** event to be raised. When a **chat_received** message is raised, it should cause the chat window to be updated in the UI.

When a new room is created or a client joins or leaves a room, a **rooms_update** message will be sent to all clients in the foyer, with the updated list. When this is received, the room data should be updated, and then a **rooms_updated** event raised. When this is raised, then the UI will respond to this by updating related content. Currently this would only make a difference in the foyer.

## Implementation Details

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










