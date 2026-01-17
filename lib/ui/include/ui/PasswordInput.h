#pragma once

#include "Widget.h"
#include <string>

namespace ui {

class PasswordInput : public Widget {
public:
    PasswordInput(int x, int y, int width, const std::string& label = "");
    ~PasswordInput() override = default;
    
    void render(WINDOW* parent) override;
    
    // Text management
    void set_text(const std::string& text);
    std::string get_text() const;
    void clear();
    
    // Input handling
    void handle_char(char ch);
    void handle_backspace();
    
    // Label
    void set_label(const std::string& label);
    std::string get_label() const;
    
    // Focus management
    void set_focus(bool focused);
    bool has_focus() const;
    
    // Mask character (default '*')
    void set_mask_char(char mask);
    char get_mask_char() const;
    
    // Cursor position
    int get_cursor_pos() const;
    int get_scroll_offset() const;

private:
    std::string text_;
    std::string label_;
    int cursor_pos_;
    int scroll_offset_;
    bool focused_;
    char mask_char_;
    
    void update_scroll();
};

using PasswordInputPtr = std::shared_ptr<PasswordInput>;

} // namespace ui
