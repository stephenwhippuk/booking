#ifndef UI_TEXTINPUT_H
#define UI_TEXTINPUT_H

#include "Widget.h"
#include "Window.h"
#include <string>
#include <functional>
#include <memory>

namespace ui {

/**
 * TextInput - Single-line text input widget
 * 
 * Accepts keyboard input when focused, displays editable text with cursor,
 * and can trigger callbacks on Enter or text changes.
 */
class TextInput : public Widget {
public:
    /**
     * Callback types
     */
    using SubmitCallback = std::function<void(const std::string&)>;
    using ChangeCallback = std::function<void(const std::string&)>;
    
    /**
     * Constructor
     */
    TextInput(int x, int y, int width);
    
    /**
     * Constructor from position and width
     */
    TextInput(const Point& pos, int width);
    
    /**
     * Destructor
     */
    virtual ~TextInput() = default;
    
    /**
     * Get the current text
     */
    std::string get_text() const { return text_; }
    
    /**
     * Set the text
     */
    void set_text(const std::string& text);
    
    /**
     * Clear the text
     */
    void clear();
    
    /**
     * Get the placeholder text (shown when empty)
     */
    std::string get_placeholder() const { return placeholder_; }
    
    /**
     * Set the placeholder text
     */
    void set_placeholder(const std::string& placeholder) {
        placeholder_ = placeholder;
    }
    
    /**
     * Get the label
     */
    std::string get_label() const { return label_; }
    
    /**
     * Set the label (displayed before input area)
     */
    void set_label(const std::string& label) {
        label_ = label;
    }
    
    /**
     * Get cursor position
     */
    size_t get_cursor_pos() const { return cursor_pos_; }
    
    /**
     * Get scroll offset
     */
    size_t get_scroll_offset() const { return scroll_offset_; }
    
    /**
     * Set max length (0 = unlimited)
     */
    void set_max_length(size_t max_length) {
        max_length_ = max_length;
    }
    
    size_t get_max_length() const { return max_length_; }
    
    /**
     * Enable/disable password mode (shows * instead of characters)
     */
    void set_password_mode(bool password_mode) {
        password_mode_ = password_mode;
    }
    
    bool is_password_mode() const { return password_mode_; }
    
    /**
     * Set callback for when Enter is pressed
     */
    void set_on_submit(SubmitCallback callback) {
        on_submit_ = callback;
    }
    
    /**
     * Set callback for when text changes
     */
    void set_on_change(ChangeCallback callback) {
        on_change_ = callback;
    }
    
    /**
     * Render the text input
     */
    void render() override;
    
    /**
     * Render with a parent window context
     */
    void render(WINDOW* parent_window);
    
    /**
     * Handle input events
     */
    bool handle_event(const Event& event) override;
    
    /**
     * Get preferred size
     */
    Size get_preferred_size() const override;
    
protected:
    void on_focus_gained() override;
    void on_focus_lost() override;
    
    /**
     * Insert character at cursor position
     */
    void insert_char(char ch);
    
    /**
     * Delete character before cursor (backspace)
     */
    void delete_char_before();
    
    /**
     * Delete character at cursor (delete)
     */
    void delete_char_at();
    
    /**
     * Move cursor left
     */
    void move_cursor_left();
    
    /**
     * Move cursor right
     */
    void move_cursor_right();
    
    /**
     * Move cursor to start
     */
    void move_cursor_home();
    
    /**
     * Move cursor to end
     */
    void move_cursor_end();
    
    /**
     * Get display text (with password masking if enabled)
     */
    std::string get_display_text() const;
    
    /**
     * Calculate visible portion of text for scrolling
     */
    std::string get_visible_text() const;
    
    std::string text_;
    std::string placeholder_;
    std::string label_;
    size_t cursor_pos_;        // Cursor position in text (0 to text_.length())
    size_t scroll_offset_;     // Horizontal scroll offset for long text
    size_t max_length_;        // Maximum text length (0 = unlimited)
    bool password_mode_;
    
    SubmitCallback on_submit_;
    ChangeCallback on_change_;
};

/**
 * Shared pointer type for text inputs
 */
using TextInputPtr = std::shared_ptr<TextInput>;

} // namespace ui

#endif // UI_TEXTINPUT_H
