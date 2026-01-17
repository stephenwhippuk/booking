# ncurses UI Component Library

A modern C++ component library for building terminal user interfaces with ncurses.

## Overview

This library provides a set of reusable UI components for building terminal applications. It uses a widget-based architecture where components can be composed together to create complex interfaces.

**Features:**
- Widget-based architecture with base `Widget` class
- Container system with `Window` for child management
- Input components: `TextInput` with cursor and scrolling
- Display components: `Label`, `Menu`, `ListBox`
- Double buffering support for flicker-free rendering
- Manual rendering pattern for reliable behavior

## Architecture

### Widget Hierarchy

```
Widget (abstract base)
├── Window (container with optional border)
├── TextInput (single-line input with label)
├── Menu (scrollable selection list)
├── Label (static text display)
└── ListBox (read-only bordered list)
```

### Rendering Pattern

Due to ncurses limitations with virtual dispatch, this library uses a **manual rendering pattern**. Instead of calling `widget->render()` directly, cast to the concrete type:

```cpp
// Manual rendering (required)
auto text_input = std::dynamic_pointer_cast<ui::TextInput>(child);
if (text_input) {
    text_input->render(parent_window);
}
```

### Double Buffering

To prevent flicker, all rendering uses ncurses double buffering:

1. **Erase**: `werase(win)` - Clear window
2. **Draw**: Render all widgets to off-screen buffer
3. **Stage**: `wnoutrefresh(win)` - Stage each window (back to front)
4. **Update**: `doupdate()` - Update physical screen once

### CMakeLists.txt

```cmake
add_subdirectory(lib/ui)
target_link_libraries(your_target ncurses_ui)
```

### Include Headers

```cpp
#include <ui/Window.h>
#include <ui/TextInput.h>
#include <ui/Menu.h>
#include <ui/Label.h>
#include <ui/ListBox.h>
```

## Usage Examples

### Complete Login Screen

```cpp
class UIManager {
    ui::WindowPtr main_window_;
    ui::TextInputPtr login_input_;
    ui::LabelPtr help_label_;

public:
    void setup_login_ui() {
        int max_y, max_x;
        getmaxyx(stdscr, max_y, max_x);
        
        int width = 50, height = 10;
        int x = (max_x - width) / 2;
        int y = (max_y - height) / 2;
        
        main_window_ = std::make_shared<ui::Window>(x, y, width, height);
        main_window_->set_bordered(true);
        main_window_->set_title("=== LOGIN ===");
        
        login_input_ = std::make_shared<ui::TextInput>(2, 3, width - 4, "Name: ");
        login_input_->set_focus(true);
        main_window_->add_child(login_input_);
        
        help_label_ = std::make_shared<ui::Label>(2, 5, "Press Enter to continue");
        main_window_->add_child(help_label_);
    }
    
    void render_login() {
        WINDOW* win = main_window_->get_window();
        werase(win);
        
        // Draw border and title
        if (main_window_->is_bordered()) {
            box(win, 0, 0);
            mvwprintw(win, 0, 2, "%s", main_window_->get_title().c_str());
        }
        
        // Render children manually
        if (login_input_) login_input_->render(win);
        if (help_label_) help_label_->render(win);
        
        // Position cursor
        if (login_input_->has_focus()) {
            int cursor_x = login_input_->get_bounds().left() + 
                          login_input_->get_label().length() + 1 +
                          login_input_->get_cursor_pos();
            int cursor_y = login_input_->get_bounds().top();
            wmove(win, cursor_y, cursor_x);
            curs_set(1);
        }
        
        wnoutrefresh(win);
        doupdate();
    }
    
    void handle_input(int ch) {
        if (login_input_ && login_input_->has_focus()) {
            if (ch == KEY_BACKSPACE || ch == 127) {
                login_input_->handle_backspace();
            } else if (ch == '\n') {
                std::string name = login_input_->get_text();
                if (!name.empty()) {
                    process_login(name);
                }
            } else if (ch >= 32 && ch < 127) {
                login_input_->handle_char(ch);
            }
        }
    }
};
```

### Menu Selection

```cpp
void setup_room_list() {
    std::vector<std::string> rooms = {"General", "Random", "Tech", "Gaming"};
    
    room_menu_ = std::make_shared<ui::Menu>(10, 10, 40, 15);
    room_menu_->set_title("Available Rooms");
    room_menu_->set_items(rooms);
}

void handle_menu_input(int ch) {
    if (ch == KEY_UP) {
        room_menu_->move_selection(-1);
    } else if (ch == KEY_DOWN) {
        room_menu_->move_selection(1);
    } else if (ch == '\n') {
        int idx = room_menu_->get_selected_index();
        if (idx >= 0) {
            join_room(rooms[idx]);
        }
    }
}
```

### Member List

```cpp
void setup_chatroom() {
    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);
    
    // Right-aligned member list
    member_list_ = std::make_shared<ui::ListBox>(max_x - 20, 0, 20, max_y - 3);
    member_list_->set_bordered(true);
    member_list_->set_title("Members");
}

void update_members(const std::vector<std::string>& members) {
    member_list_->set_items(members);
}

void render_chatroom() {
    werase(stdscr);
    
    // Render chat messages, input, etc.
    render_chat_messages();
    render_chat_input();
    
    // Render member list on right
    member_list_->render(stdscr);
    
    wnoutrefresh(stdscr);
    doupdate();
}
```

## Best Practices

### 1. Always Clear Before Drawing
```cpp
werase(win);  // Clear window
// ... render widgets ...
wnoutrefresh(win);
```

### 2. Use Double Buffering
```cpp
// Stage all windows (back to front)
wnoutrefresh(stdscr);
wnoutrefresh(window1);
wnoutrefresh(dialog);

// Update screen once
doupdate();
```

### 3. Manual Type Casting for Rendering
```cpp
for (const auto& child : window->get_children()) {
    if (!child->is_visible()) continue;
    
    if (auto label = std::dynamic_pointer_cast<ui::Label>(child)) {
        label->render(parent_win);
    } else if (auto input = std::dynamic_pointer_cast<ui::TextInput>(child)) {
        input->render(parent_win);
    } else if (auto menu = std::dynamic_pointer_cast<ui::Menu>(child)) {
        menu->render(parent_win);
    }
}
```

### 4. Cursor Management
```cpp
if (text_input && text_input->has_focus()) {
    int x = calc_cursor_x();
    int y = calc_cursor_y();
    wmove(win, y, x);
    curs_set(1);  // Show cursor
} else {
    curs_set(0);  // Hide cursor
}
```

### 5. Proper Render Order
Render from background to foreground:
```cpp
// 1. Background/main content
werase(stdscr);
render_background(stdscr);
wnoutrefresh(stdscr);

// 2. Dialogs/overlays
render_dialog(dialog_win);
wnoutrefresh(dialog_win);

// 3. Update screen
doupdate();
```

## Common Pitfalls

### ❌ Virtual Dispatch Won't Work Reliably
```cpp
// Don't do this:
widget->render(win);  // May not call the right method
```

### ✅ Always Cast to Concrete Type
```cpp
// Do this:
if (auto menu = std::dynamic_pointer_cast<ui::Menu>(widget)) {
    menu->render(win);
}
```

### ❌ Missing doupdate()
```cpp
wnoutrefresh(win1);
wnoutrefresh(win2);
// Screen not updated!
```

### ✅ Call doupdate() After Staging
```cpp
wnoutrefresh(win1);
wnoutrefresh(win2);
doupdate();  // Now updates
```

### ❌ Wrong Staging Order
```cpp
wnoutrefresh(dialog);   // Front
wnoutrefresh(stdscr);   // Back - will cover dialog!
```

### ✅ Stage Back to Front
```cpp
wnoutrefresh(stdscr);   // Back
wnoutrefresh(dialog);   // Front
```

### ❌ Forgetting to Clear
```cpp
// Old content remains visible
box(win, 0, 0);
render_widgets();
```

### ✅ Always Clear First
```cpp
werase(win);
box(win, 0, 0);
render_widgets();
```

## Testing

Build and run tests:
```bash
cmake -B build
cmake --build build
./build/tests
```

## Real-World Example

See `src/client/UIManager.cpp` for a complete implementation featuring:
- Login screen with TextInput
- Foyer with Menu for room selection
- Chatroom with TextInput for messages
- Member ListBox showing online users
- Room creation dialog

## Design Notes

### Why Manual Rendering?

The ncurses library has issues with virtual function dispatch across shared libraries. To work around this, we use manual type casting with `std::dynamic_pointer_cast` for rendering. While less elegant than pure polymorphism, this approach is reliable and explicit.

### Why Double Buffering?

Calling `wrefresh()` after every widget would cause significant flicker. Instead, we:
1. Stage all changes with `wnoutrefresh()` (writes to memory buffer)
2. Update the physical screen once with `doupdate()` (single refresh)

This produces smooth, flicker-free rendering.

## Future Enhancements

Potential additions:
- ✨ Dialog widget for modals
- ✨ Progress bar component
- ✨ Table/Grid widget for tabular data
- ✨ Color theme support
- ✨ Mouse input handling
- ✨ Flexible layout managers

## License

Part of the booking chat application.


### Window

Container that can hold child widgets with optional border and title.

**Features:**
- Child widget management
- Optional border with title
- Visibility control
- Fullscreen or bounded positioning

**Example:**
```cpp
auto window = std::make_shared<ui::Window>(10, 10, 60, 20);
window->set_bordered(true);
window->set_title("Login");

auto label = std::make_shared<ui::Label>(2, 2, "Enter name:");
window->add_child(label);
```

**API:**
- `Window(x, y, width, height)` - Create at position with size
- `set_bordered(bool)` - Enable/disable border
- `set_title(string)` - Set border title
- `add_child(widget)` - Add child widget
- `get_children()` - Get all children
- `get_window()` - Get underlying `WINDOW*`

### TextInput

Single-line text input with inline label, cursor, and horizontal scrolling.

**Features:**
- Inline label (e.g., "Name: ____")
- Cursor positioning and display
- Horizontal scrolling for long text
- Focus management
- Text manipulation (insert, backspace)

**Example:**
```cpp
auto input = std::make_shared<ui::TextInput>(2, 2, 40, "Name: ");
input->set_focus(true);
input->set_text("Alice");

// Input handling:
if (ch == KEY_BACKSPACE) {
    input->handle_backspace();
} else if (ch >= 32 && ch < 127) {
    input->handle_char(ch);
}
```

**API:**
- `TextInput(x, y, width, label)` - Create input field
- `set_text(string)`, `get_text()` - Text management
- `set_focus(bool)`, `has_focus()` - Focus control
- `handle_char(ch)` - Add character at cursor
- `handle_backspace()` - Delete character before cursor
- `get_cursor_pos()` - Get cursor position
- `get_scroll_offset()` - Get scroll offset for long text
- `render(WINDOW*)` - Draw to window

### Menu

Vertical selection list with keyboard navigation and scrolling.

**Features:**
- Scrollable item list
- Keyboard navigation (Up/Down)
- Selection highlighting (reverse video)
- Automatic scrolling for long lists
- Bordered display with title

**Example:**
```cpp
std::vector<std::string> items = {"General", "Random", "Help"};
auto menu = std::make_shared<ui::Menu>(2, 4, 40, 10);
menu->set_items(items);
menu->set_title("Available Rooms");

// Navigation:
menu->move_selection(-1);  // Move up
menu->move_selection(1);   // Move down
int selected = menu->get_selected_index();
```

**API:**
- `Menu(x, y, width, height)` - Create menu
- `set_items(vector<string>)` - Set menu items
- `set_title(string)` - Set border title
- `move_selection(delta)` - Move selection (-1 up, +1 down)
- `get_selected_index()` - Get current selection
- `render(WINDOW*)` - Draw to window

### Label

Static text display component.

**Features:**
- Single or multi-line text
- Attribute support (bold, reverse, etc.)
- Visibility control

**Example:**
```cpp
auto label = std::make_shared<ui::Label>(2, 2, "Welcome!");
label->set_text("Welcome, " + username + "!");
label->render(stdscr);
```

**API:**
- `Label(x, y, text)` - Create label
- `set_text(string)`, `get_text()` - Text management
- `render(WINDOW*)` - Draw to window

### ListBox

Read-only scrollable list with border, title, and automatic truncation.

**Features:**
- Bordered display
- Title support
- Automatic item truncation to fit width
- Scroll indicator for long lists
- Empty state handling

**Example:**
```cpp
auto list = std::make_shared<ui::ListBox>(60, 0, 20, 30);
list->set_bordered(true);
list->set_title("Members");

std::vector<std::string> members = {"Alice", "Bob", "Charlie"};
list->set_items(members);
list->render(stdscr);
```

**API:**
- `ListBox(x, y, width, height)` - Create list box
- `set_bordered(bool)` - Enable/disable border
- `set_title(string)` - Set title
- `set_items(vector<string>)` - Update list items
- `render(WINDOW*)` - Draw to window

## Integration

### CMakeLists.txt

```cmake
add_subdirectory(lib/ui)
target_link_libraries(your_target ncurses_ui)
#include <ui/Window.h>
#include <ui/Container.h>
```

## Building

The library is built as part of the parent project's build system.

```bash
mkdir build && cd build
cmake ..
make ncurses_ui
```

## License

MIT
