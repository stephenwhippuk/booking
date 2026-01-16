#include "ui/TextInput.h"
#include <algorithm>
#include <ncurses.h>

namespace ui {

TextInput::TextInput(int x, int y, int width)
    : cursor_pos_(0)
    , scroll_offset_(0)
    , max_length_(0)
    , password_mode_(false)
{
    bounds_ = Rect(x, y, width, 1);  // Single line height
    focusable_ = true;
}

TextInput::TextInput(const Point& pos, int width)
    : TextInput(pos.x, pos.y, width)
{
}

void TextInput::set_text(const std::string& text) {
    text_ = text;
    
    // Enforce max length
    if (max_length_ > 0 && text_.length() > max_length_) {
        text_ = text_.substr(0, max_length_);
    }
    
    // Clamp cursor position
    cursor_pos_ = std::min(cursor_pos_, text_.length());
    
    if (on_change_) {
        on_change_(text_);
    }
}

void TextInput::clear() {
    text_.clear();
    cursor_pos_ = 0;
    scroll_offset_ = 0;
    
    if (on_change_) {
        on_change_(text_);
    }
}

void TextInput::render() {
    if (!visible_) {
        return;
    }
    
    // Render directly to stdscr
    render(stdscr);
}

void TextInput::render(WINDOW* parent_window) {
    if (!visible_ || !parent_window) {
        return;
    }
    
    int x = bounds_.left();
    int y = bounds_.top();
    int width = bounds_.size.width;
    
    // Draw label if present
    if (!label_.empty()) {
        mvwprintw(parent_window, y, x, "%s", label_.c_str());
        x += label_.length() + 1;  // Add space after label
        width -= (label_.length() + 1);
    }
    
    // Draw input area
    std::string display_text = get_visible_text();
    
    // Show placeholder if empty and not focused
    if (text_.empty() && !focused_ && !placeholder_.empty()) {
        wattron(parent_window, A_DIM);
        mvwprintw(parent_window, y, x, "%.*s", width, placeholder_.c_str());
        wattroff(parent_window, A_DIM);
    } else {
        // Show regular or masked text
        if (focused_) {
            wattron(parent_window, A_REVERSE);
        }
        
        // Clear the input area first and draw text
        mvwprintw(parent_window, y, x, "%*s", width, "");  // Clear with spaces
        mvwprintw(parent_window, y, x, "%.*s", width, display_text.c_str());
        
        if (focused_) {
            wattroff(parent_window, A_REVERSE);
            
            // Position cursor
            int cursor_x = x + (cursor_pos_ - scroll_offset_);
            wmove(parent_window, y, cursor_x);
        }
    }
}

bool TextInput::handle_event(const Event& event) {
    if (!visible_ || !focused_ || event.type != EventType::KEY_PRESS) {
        return false;
    }
    
    int key = event.key;
    
    // Handle special keys
    if (key == '\n' || key == KEY_ENTER) {
        // Submit
        if (on_submit_) {
            on_submit_(text_);
        }
        return true;
    }
    
    if (key == KEY_BACKSPACE || key == 127 || key == 8) {
        delete_char_before();
        return true;
    }
    
    if (key == KEY_DC) {
        delete_char_at();
        return true;
    }
    
    if (key == KEY_LEFT) {
        move_cursor_left();
        return true;
    }
    
    if (key == KEY_RIGHT) {
        move_cursor_right();
        return true;
    }
    
    if (key == KEY_HOME) {
        move_cursor_home();
        return true;
    }
    
    if (key == KEY_END) {
        move_cursor_end();
        return true;
    }
    
    // Handle printable characters
    if (key >= 32 && key < 127) {
        insert_char(static_cast<char>(key));
        return true;
    }
    
    return false;
}

Size TextInput::get_preferred_size() const {
    int width = bounds_.size.width;
    if (!label_.empty()) {
        width = std::max(width, static_cast<int>(label_.length()) + 20);
    }
    return Size(width, 1);
}

void TextInput::on_focus_gained() {
    // Show cursor
    curs_set(1);
}

void TextInput::on_focus_lost() {
    // Hide cursor
    curs_set(0);
}

void TextInput::insert_char(char ch) {
    // Check max length
    if (max_length_ > 0 && text_.length() >= max_length_) {
        return;
    }
    
    text_.insert(cursor_pos_, 1, ch);
    cursor_pos_++;
    
    // Adjust scroll if cursor goes off screen
    int visible_width = bounds_.size.width;
    if (!label_.empty()) {
        visible_width -= label_.length();
    }
    
    if (cursor_pos_ - scroll_offset_ >= static_cast<size_t>(visible_width)) {
        scroll_offset_ = cursor_pos_ - visible_width + 1;
    }
    
    if (on_change_) {
        on_change_(text_);
    }
}

void TextInput::delete_char_before() {
    if (cursor_pos_ > 0) {
        text_.erase(cursor_pos_ - 1, 1);
        cursor_pos_--;
        
        // Adjust scroll
        if (scroll_offset_ > 0 && cursor_pos_ < scroll_offset_) {
            scroll_offset_ = cursor_pos_;
        }
        
        if (on_change_) {
            on_change_(text_);
        }
    }
}

void TextInput::delete_char_at() {
    if (cursor_pos_ < text_.length()) {
        text_.erase(cursor_pos_, 1);
        
        if (on_change_) {
            on_change_(text_);
        }
    }
}

void TextInput::move_cursor_left() {
    if (cursor_pos_ > 0) {
        cursor_pos_--;
        
        // Adjust scroll
        if (cursor_pos_ < scroll_offset_) {
            scroll_offset_ = cursor_pos_;
        }
    }
}

void TextInput::move_cursor_right() {
    if (cursor_pos_ < text_.length()) {
        cursor_pos_++;
        
        // Adjust scroll
        int visible_width = bounds_.size.width;
        if (!label_.empty()) {
            visible_width -= label_.length();
        }
        
        if (cursor_pos_ - scroll_offset_ >= static_cast<size_t>(visible_width)) {
            scroll_offset_ = cursor_pos_ - visible_width + 1;
        }
    }
}

void TextInput::move_cursor_home() {
    cursor_pos_ = 0;
    scroll_offset_ = 0;
}

void TextInput::move_cursor_end() {
    cursor_pos_ = text_.length();
    
    // Adjust scroll to show end
    int visible_width = bounds_.size.width;
    if (!label_.empty()) {
        visible_width -= label_.length();
    }
    
    if (text_.length() > static_cast<size_t>(visible_width)) {
        scroll_offset_ = text_.length() - visible_width;
    }
}

std::string TextInput::get_display_text() const {
    if (password_mode_ && !text_.empty()) {
        return std::string(text_.length(), '*');
    }
    return text_;
}

std::string TextInput::get_visible_text() const {
    std::string display = get_display_text();
    
    int visible_width = bounds_.size.width;
    if (!label_.empty()) {
        visible_width -= label_.length();
    }
    
    if (display.length() <= static_cast<size_t>(visible_width)) {
        return display;
    }
    
    return display.substr(scroll_offset_, visible_width);
}

} // namespace ui
