#include "ui/PasswordInput.h"
#include <ncurses.h>
#include <algorithm>

namespace ui {

PasswordInput::PasswordInput(int x, int y, int width, const std::string& label)
    : Widget(x, y, width, 1)
    , label_(label)
    , cursor_pos_(0)
    , scroll_offset_(0)
    , focused_(false)
    , mask_char_('*')
{
}

void PasswordInput::render(WINDOW* parent) {
    if (!visible_ || !parent) return;
    
    int y = bounds_.top();
    int x = bounds_.left();
    int width = bounds_.width();
    
    // Render label
    if (!label_.empty()) {
        mvwprintw(parent, y, x, "%s", label_.c_str());
        x += label_.length();
        width -= label_.length();
    }
    
    // Calculate visible portion of masked text
    int input_width = width - 1;  // Leave space for cursor
    std::string masked_text(text_.length(), mask_char_);
    
    // Apply scrolling
    int visible_start = scroll_offset_;
    int visible_end = std::min(visible_start + input_width, static_cast<int>(masked_text.length()));
    std::string visible_text = masked_text.substr(visible_start, visible_end - visible_start);
    
    // Render masked input field
    if (focused_) {
        wattron(parent, A_UNDERLINE);
    }
    
    // Clear the input area and draw masked text
    for (int i = 0; i < input_width; ++i) {
        if (i < static_cast<int>(visible_text.length())) {
            mvwaddch(parent, y, x + i, visible_text[i]);
        } else {
            mvwaddch(parent, y, x + i, ' ');
        }
    }
    
    if (focused_) {
        wattroff(parent, A_UNDERLINE);
    }
}

void PasswordInput::set_text(const std::string& text) {
    text_ = text;
    cursor_pos_ = text_.length();
    update_scroll();
}

std::string PasswordInput::get_text() const {
    return text_;
}

void PasswordInput::clear() {
    text_.clear();
    cursor_pos_ = 0;
    scroll_offset_ = 0;
}

void PasswordInput::handle_char(char ch) {
    if (ch >= 32 && ch < 127) {  // Printable ASCII
        text_.insert(cursor_pos_, 1, ch);
        cursor_pos_++;
        update_scroll();
    }
}

void PasswordInput::handle_backspace() {
    if (cursor_pos_ > 0) {
        text_.erase(cursor_pos_ - 1, 1);
        cursor_pos_--;
        update_scroll();
    }
}

void PasswordInput::set_label(const std::string& label) {
    label_ = label;
}

std::string PasswordInput::get_label() const {
    return label_;
}

void PasswordInput::set_focus(bool focused) {
    focused_ = focused;
}

bool PasswordInput::has_focus() const {
    return focused_;
}

void PasswordInput::set_mask_char(char mask) {
    mask_char_ = mask;
}

char PasswordInput::get_mask_char() const {
    return mask_char_;
}

int PasswordInput::get_cursor_pos() const {
    return cursor_pos_;
}

int PasswordInput::get_scroll_offset() const {
    return scroll_offset_;
}

void PasswordInput::update_scroll() {
    int input_width = bounds_.width();
    if (!label_.empty()) {
        input_width -= label_.length();
    }
    input_width -= 1;  // Leave space for cursor
    
    // Scroll right if cursor is beyond visible area
    if (cursor_pos_ >= scroll_offset_ + input_width) {
        scroll_offset_ = cursor_pos_ - input_width + 1;
    }
    
    // Scroll left if cursor is before visible area
    if (cursor_pos_ < scroll_offset_) {
        scroll_offset_ = cursor_pos_;
    }
    
    // Clamp scroll offset
    if (scroll_offset_ < 0) {
        scroll_offset_ = 0;
    }
}

} // namespace ui
