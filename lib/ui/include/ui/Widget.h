#ifndef UI_WIDGET_H
#define UI_WIDGET_H

#include "Types.h"
#include <memory>
#include <string>
#include <ncurses.h>

namespace ui {

/**
 * Widget - Base class for all UI components
 * 
 * Provides the fundamental interface that all UI components must implement.
 * Handles positioning, sizing, rendering, and event processing.
 */
class Widget {
public:
    virtual ~Widget() = default;
    
    /**
     * Get the widget's bounding rectangle
     */
    virtual Rect get_bounds() const { return bounds_; }
    
    /**
     * Set the widget's bounding rectangle
     */
    virtual void set_bounds(const Rect& bounds) { 
        bounds_ = bounds;
        on_resize();
    }
    
    /**
     * Get the widget's position
     */
    virtual Point get_position() const { return bounds_.position; }
    
    /**
     * Set the widget's position
     */
    virtual void set_position(const Point& pos) {
        bounds_.position = pos;
    }
    
    /**
     * Get the widget's size
     */
    virtual Size get_size() const { return bounds_.size; }
    
    /**
     * Set the widget's size
     */
    virtual void set_size(const Size& size) {
        bounds_.size = size;
        on_resize();
    }
    
    /**
     * Check if widget is visible
     */
    virtual bool is_visible() const { return visible_; }
    
    /**
     * Set visibility
     */
    virtual void set_visible(bool visible) { visible_ = visible; }
    
    /**
     * Check if widget can receive focus
     */
    virtual bool can_focus() const { return focusable_; }
    
    /**
     * Set if widget can receive focus
     */
    virtual void set_focusable(bool focusable) { focusable_ = focusable; }
    
    /**
     * Check if widget currently has focus
     */
    virtual bool has_focus() const { return focused_; }
    
    /**
     * Set focus state
     */
    virtual void set_focus(bool focus) {
        if (focused_ != focus && focusable_) {
            focused_ = focus;
            if (focused_) {
                on_focus_gained();
            } else {
                on_focus_lost();
            }
        }
    }
    
    /**
     * Get the widget's style
     */
    virtual Style get_style() const { return style_; }
    
    /**
     * Set the widget's style
     */
    virtual void set_style(const Style& style) { style_ = style; }
    
    /**
     * Get layout constraints
     */
    virtual Constraints get_constraints() const { return constraints_; }
    
    /**
     * Set layout constraints
     */
    virtual void set_constraints(const Constraints& constraints) {
        constraints_ = constraints;
    }
    
    /**
     * Calculate preferred size based on content
     */
    virtual Size get_preferred_size() const {
        return Size(constraints_.min_width, constraints_.min_height);
    }
    
    /**
     * Render the widget to the screen
     * Must be implemented by derived classes
     */
    virtual void render() = 0;
    
    /**
     * Handle input events
     * Returns true if event was handled
     */
    virtual bool handle_event(const Event& event) {
        return false;  // Base implementation doesn't handle events
    }
    
    /**
     * Update widget state (called each frame)
     */
    virtual void update() {}
    
protected:
    /**
     * Called when widget is resized
     */
    virtual void on_resize() {}
    
    /**
     * Called when widget gains focus
     */
    virtual void on_focus_gained() {}
    
    /**
     * Called when widget loses focus
     */
    virtual void on_focus_lost() {}
    
    // Widget properties
    Rect bounds_;
    Style style_;
    Constraints constraints_;
    bool visible_ = true;
    bool focusable_ = false;
    bool focused_ = false;
};

/**
 * Shared pointer type for widgets
 */
using WidgetPtr = std::shared_ptr<Widget>;

} // namespace ui

#endif // UI_WIDGET_H
