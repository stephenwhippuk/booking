#ifndef UI_WINDOW_H
#define UI_WINDOW_H

#include "Widget.h"
#include <ncurses.h>
#include <string>
#include <memory>
#include <vector>

namespace ui {

/**
 * Window - Manages an ncurses WINDOW* and provides drawing context
 * 
 * A Window represents a rectangular area on the screen with its own
 * coordinate system and rendering context. It can have borders, titles,
 * and manages double buffering with wnoutrefresh/doupdate.
 * Can also act as a container for child widgets.
 */
class Window : public Widget {
public:
    /**
     * Constructor - creates a new ncurses window
     */
    Window(int x, int y, int width, int height);
    
    /**
     * Constructor from Rect
     */
    explicit Window(const Rect& bounds);
    
    /**
     * Destructor - cleans up ncurses window
     */
    virtual ~Window();
    
    // Prevent copying (WINDOW* is not copyable)
    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;
    
    /**
     * Get the underlying ncurses WINDOW*
     */
    WINDOW* get_window() const { return window_; }
    
    /**
     * Get the content area (area inside border if bordered)
     */
    Rect get_content_rect() const;
    
    /**
     * Set window title (displayed in border if bordered)
     */
    void set_title(const std::string& title) { 
        title_ = title;
    }
    
    std::string get_title() const { return title_; }
    
    /**
     * Enable/disable border
     */
    void set_bordered(bool bordered) { 
        bordered_ = bordered;
    }
    
    bool is_bordered() const { return bordered_; }
    
    /**
     * Clear the window contents
     */
    void clear();
    
    /**
     * Erase window contents (more thorough than clear)
     */
    void erase();
    
    /**
     * Draw a box border around the window
     */
    void draw_border();
    
    /**
     * Draw text at position (relative to window)
     */
    void draw_text(int x, int y, const std::string& text);
    
    /**
     * Draw text with style
     */
    void draw_text(int x, int y, const std::string& text, const Style& style);
    
    /**
     * Draw text with attributes
     */
    void draw_text_attr(int x, int y, const std::string& text, int attr);
    
    /**
     * Move cursor to position
     */
    void move_cursor(int x, int y);
    
    /**
     * Set cursor visibility (0=invisible, 1=normal, 2=very visible)
     */
    void set_cursor_visible(int visibility);
    
    /**
     * Enable scrolling for this window
     */
    void set_scrollable(bool scrollable);
    
    /**
     * Add a child widget to the window
     */
    void add_child(WidgetPtr child);
    
    /**
     * Remove a child widget
     */
    void remove_child(WidgetPtr child);
    
    /**
     * Clear all children
     */
    void clear_children();
    
    /**
     * Get all children
     */
    const std::vector<WidgetPtr>& get_children() const { return children_; }
    
    /**
     * Set focus to a specific child widget
     */
    void focus_child(WidgetPtr child);
    
    /**
     * Focus next focusable child
     */
    void focus_next_child();
    
    /**
     * Focus previous focusable child
     */
    void focus_previous_child();
    
    /**
     * Get the currently focused child (nullptr if none)
     */
    WidgetPtr get_focused_child() const { return focused_child_; }
    
    /**
     * Render the window (calls wnoutrefresh)
     * Actual screen update happens with doupdate()
     */
    void render() override;
    
    /**
     * Refresh immediately (useful for single window updates)
     */
    void refresh_now();
    
    /**
     * Resize and reposition the window
     */
    void set_bounds(const Rect& bounds) override;
    
    /**
     * Handle input events (propagates to focused child)
     */
    bool handle_event(const Event& event) override;
    
protected:
    /**
     * Called when window is resized
     */
    void on_resize() override;
    
    /**
     * Recreate the ncurses window with new dimensions
     */
    void recreate_window();
    
    /**
     * Render all children
     */
    void render_children();
    
    WINDOW* window_;
    std::string title_;
    bool bordered_;
    std::vector<WidgetPtr> children_;
    WidgetPtr focused_child_;
};

/**
 * Shared pointer type for windows
 */
using WindowPtr = std::shared_ptr<Window>;

} // namespace ui

#endif // UI_WINDOW_H
