#include <gtest/gtest.h>
#include "ApplicationManager.h"
#include "ThreadSafeQueue.h"
#include "UICommand.h"
#include <thread>
#include <chrono>

using namespace std::chrono_literals;

class ApplicationManagerTest : public ::testing::Test {
protected:
    ThreadSafeQueue<std::string> network_inbound;
    ThreadSafeQueue<std::string> network_outbound;
    ThreadSafeQueue<UICommand> ui_commands;
    ThreadSafeQueue<std::string> input_events;
    
    std::unique_ptr<ApplicationManager> app_manager;
    
    void SetUp() override {
        app_manager = std::make_unique<ApplicationManager>(
            network_inbound,
            network_outbound,
            ui_commands,
            input_events
        );
        app_manager->start();
        std::this_thread::sleep_for(50ms);  // Let thread start
    }
    
    void TearDown() override {
        app_manager->stop();
    }
    
    // Helper to get UI commands of a specific type
    std::vector<UICommand> drain_ui_commands(int timeout_ms = 100) {
        std::vector<UICommand> commands;
        UICommand cmd(UICommandType::QUIT);
        while (ui_commands.try_pop(cmd, std::chrono::milliseconds(timeout_ms))) {
            commands.push_back(cmd);
            timeout_ms = 10;  // Shorter timeout for subsequent commands
        }
        return commands;
    }
};

TEST_F(ApplicationManagerTest, InitialState) {
    const auto& state = app_manager->get_state();
    
    EXPECT_FALSE(state.is_connected());
    EXPECT_EQ(state.get_screen(), ApplicationState::Screen::LOGIN);
    EXPECT_TRUE(state.get_username().empty());
    EXPECT_TRUE(state.get_rooms().empty());
}

TEST_F(ApplicationManagerTest, LoginFlow) {
    // Simulate user login input
    input_events.push("LOGIN:TestUser");
    
    // Should send username to network
    std::string net_msg;
    ASSERT_TRUE(network_outbound.try_pop(net_msg, 500ms));
    EXPECT_EQ(net_msg, "TestUser\n");
    
    // Check state updated
    const auto& state = app_manager->get_state();
    EXPECT_TRUE(state.is_connected());
    EXPECT_EQ(state.get_username(), "TestUser");
}

TEST_F(ApplicationManagerTest, RoomListProcessing) {
    // Simulate server sending room list
    std::string room_list = 
        "ROOM_LIST\n"
        "General|3\n"
        "Gaming|5\n"
        "END_ROOM_LIST\n";
    
    network_inbound.push(room_list);
    
    // Should generate UI commands
    auto commands = drain_ui_commands();
    
    ASSERT_GE(commands.size(), 2);
    
    // Should show foyer
    bool has_show_foyer = false;
    bool has_update_list = false;
    
    for (const auto& cmd : commands) {
        if (cmd.type == UICommandType::SHOW_FOYER) {
            has_show_foyer = true;
        }
        if (cmd.type == UICommandType::UPDATE_ROOM_LIST) {
            has_update_list = true;
            auto rooms = cmd.get<RoomListData>().rooms;
            EXPECT_EQ(rooms.size(), 2);
            EXPECT_EQ(rooms[0].name, "General");
            EXPECT_EQ(rooms[0].client_count, 3);
            EXPECT_EQ(rooms[1].name, "Gaming");
            EXPECT_EQ(rooms[1].client_count, 5);
        }
    }
    
    EXPECT_TRUE(has_show_foyer);
    EXPECT_TRUE(has_update_list);
    
    // Check state
    const auto& state = app_manager->get_state();
    EXPECT_EQ(state.get_screen(), ApplicationState::Screen::FOYER);
    EXPECT_EQ(state.get_rooms().size(), 2);
}

TEST_F(ApplicationManagerTest, JoinRoom) {
    // User selects a room
    input_events.push("ROOM_SELECTED:General");
    
    // Should send JOIN_ROOM command
    std::string net_msg;
    ASSERT_TRUE(network_outbound.try_pop(net_msg, 500ms));
    EXPECT_EQ(net_msg, "JOIN_ROOM:General\n");
    
    // Simulate server response
    network_inbound.push("JOINED_ROOM:General\n");
    
    // Should generate UI command to show chatroom
    auto commands = drain_ui_commands();
    
    bool has_show_chatroom = false;
    for (const auto& cmd : commands) {
        if (cmd.type == UICommandType::SHOW_CHATROOM) {
            has_show_chatroom = true;
        }
    }
    
    EXPECT_TRUE(has_show_chatroom);
    
    // Check state
    const auto& state = app_manager->get_state();
    EXPECT_EQ(state.get_screen(), ApplicationState::Screen::CHATROOM);
    EXPECT_EQ(state.get_current_room(), "General");
}

TEST_F(ApplicationManagerTest, ChatMessages) {
    // First join a room
    network_inbound.push("JOINED_ROOM:Test\n");
    std::this_thread::sleep_for(50ms);
    drain_ui_commands();  // Clear join commands
    
    // Receive chat message
    network_inbound.push("CHAT:Alice: Hello everyone!\n");
    
    // Should generate ADD_CHAT_MESSAGE command
    auto commands = drain_ui_commands();
    
    bool has_chat_msg = false;
    for (const auto& cmd : commands) {
        if (cmd.type == UICommandType::ADD_CHAT_MESSAGE) {
            has_chat_msg = true;
            auto msg = cmd.get<ChatMessageData>().message;
            EXPECT_EQ(msg, "Alice: Hello everyone!");
        }
    }
    
    EXPECT_TRUE(has_chat_msg);
    
    // Check state
    const auto& state = app_manager->get_state();
    auto messages = state.get_chat_messages();
    ASSERT_EQ(messages.size(), 1);
    EXPECT_EQ(messages[0], "Alice: Hello everyone!");
}

TEST_F(ApplicationManagerTest, SendChatMessage) {
    // Join room first
    network_inbound.push("JOINED_ROOM:Test\n");
    std::this_thread::sleep_for(50ms);
    
    // User sends chat message
    input_events.push("CHAT_MESSAGE:Hello, world!");
    
    // Should send to network
    std::string net_msg;
    ASSERT_TRUE(network_outbound.try_pop(net_msg, 500ms));
    EXPECT_EQ(net_msg, "Hello, world!\n");
}

TEST_F(ApplicationManagerTest, LeaveRoom) {
    // Join room first
    network_inbound.push("JOINED_ROOM:Test\n");
    std::this_thread::sleep_for(50ms);
    drain_ui_commands();
    
    // User leaves room
    input_events.push("LEAVE");
    
    // Should send /leave
    std::string net_msg;
    ASSERT_TRUE(network_outbound.try_pop(net_msg, 500ms));
    EXPECT_EQ(net_msg, "/leave\n");
    
    // Simulate server response
    network_inbound.push("LEFT_ROOM\nROOM_LIST\nGeneral|2\nEND_ROOM_LIST\n");
    
    // Should show foyer
    auto commands = drain_ui_commands();
    
    bool has_show_foyer = false;
    for (const auto& cmd : commands) {
        if (cmd.type == UICommandType::SHOW_FOYER) {
            has_show_foyer = true;
        }
    }
    
    EXPECT_TRUE(has_show_foyer);
    
    // Check state
    const auto& state = app_manager->get_state();
    EXPECT_EQ(state.get_screen(), ApplicationState::Screen::FOYER);
    EXPECT_TRUE(state.get_current_room().empty());
}

TEST_F(ApplicationManagerTest, CreateRoom) {
    // User creates room
    input_events.push("CREATE_ROOM:MyRoom");
    
    // Should send CREATE_ROOM command
    std::string net_msg;
    ASSERT_TRUE(network_outbound.try_pop(net_msg, 500ms));
    EXPECT_EQ(net_msg, "CREATE_ROOM:MyRoom\n");
}

TEST_F(ApplicationManagerTest, ErrorHandling) {
    // Simulate room exists error
    network_inbound.push("ROOM_EXISTS\n");
    
    // Should generate error command
    auto commands = drain_ui_commands();
    
    bool has_error = false;
    for (const auto& cmd : commands) {
        if (cmd.type == UICommandType::SHOW_ERROR) {
            has_error = true;
            auto error = cmd.get<ErrorData>().message;
            EXPECT_EQ(error, "Room already exists");
        }
    }
    
    EXPECT_TRUE(has_error);
}

TEST_F(ApplicationManagerTest, ConnectionLost) {
    // Set up as connected
    input_events.push("LOGIN:TestUser");
    std::this_thread::sleep_for(50ms);
    drain_ui_commands();
    
    // Simulate connection lost
    network_inbound.push("SERVER_DISCONNECTED\n");
    
    // Should show login screen and error
    auto commands = drain_ui_commands();
    
    bool has_show_login = false;
    bool has_error = false;
    
    for (const auto& cmd : commands) {
        if (cmd.type == UICommandType::SHOW_LOGIN) {
            has_show_login = true;
        }
        if (cmd.type == UICommandType::SHOW_ERROR) {
            has_error = true;
        }
    }
    
    EXPECT_TRUE(has_show_login);
    EXPECT_TRUE(has_error);
    
    // Check state
    const auto& state = app_manager->get_state();
    EXPECT_FALSE(state.is_connected());
    EXPECT_EQ(state.get_screen(), ApplicationState::Screen::LOGIN);
}

TEST_F(ApplicationManagerTest, Logout) {
    // Set up as connected
    input_events.push("LOGIN:TestUser");
    std::this_thread::sleep_for(50ms);
    
    // Drain login network message
    std::string dummy;
    network_outbound.try_pop(dummy, 100ms);
    
    drain_ui_commands();
    
    // User logs out
    input_events.push("LOGOUT");
    
    // Should send /logout
    std::string net_msg;
    ASSERT_TRUE(network_outbound.try_pop(net_msg, 500ms));
    EXPECT_EQ(net_msg, "/logout\n");
    
    // Should show login
    auto commands = drain_ui_commands();
    bool has_show_login = false;
    for (const auto& cmd : commands) {
        if (cmd.type == UICommandType::SHOW_LOGIN) {
            has_show_login = true;
        }
    }
    EXPECT_TRUE(has_show_login);
    
    // Check state reset
    const auto& state = app_manager->get_state();
    EXPECT_FALSE(state.is_connected());
    EXPECT_EQ(state.get_screen(), ApplicationState::Screen::LOGIN);
}

TEST_F(ApplicationManagerTest, QuitCommand) {
    input_events.push("QUIT");
    
    // Should generate QUIT UI command
    auto commands = drain_ui_commands();
    
    bool has_quit = false;
    for (const auto& cmd : commands) {
        if (cmd.type == UICommandType::QUIT) {
            has_quit = true;
        }
    }
    
    EXPECT_TRUE(has_quit);
    
    // Application should stop
    std::this_thread::sleep_for(100ms);
    EXPECT_FALSE(app_manager->is_running());
}
