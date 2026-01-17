#pragma once

#include "Widget.h"
#include <string>
#include <functional>
#include <vector>

namespace ui {

/**
 * MessageBox - A modal dialog that displays a message and closes on Enter
 * 
 * Features:
 * - Centered on screen
 * - Border with title
 * - Multi-line message support
 * - Closes on Enter key
 * - Optional callback when closed
 */
class MessageBox : public Widget {
public:
    MessageBox(int width, int height, const std::string& title, const std::string& message);
    
    void render() override;  // Pure virtual from Widget
    void render(WINDOW* parent);  // Additional method for explicit parent
    bool handle_event(const Event& event) override;
    
    void set_message(const std::string& message);
    void set_title(const std::string& title);
    void set_on_close(std::function<void()> callback);
    void set_visible(bool visible) { visible_ = visible; }
    bool is_visible() const { return visible_; }
    
private:
    std::string title_;
    std::string message_;
    int width_;
    int height_;
    bool visible_;
    std::function<void()> on_close_;
    
    std::vector<std::string> wrap_text(const std::string& text, int max_width);
};

} // namespace ui
