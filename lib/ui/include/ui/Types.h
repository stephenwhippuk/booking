#ifndef UI_TYPES_H
#define UI_TYPES_H

#include <cstdint>
#include <string>

namespace ui {

/**
 * Position in terminal coordinates (0-based)
 */
struct Point {
    int x;
    int y;
    
    Point(int x = 0, int y = 0) : x(x), y(y) {}
};

/**
 * Size dimensions
 */
struct Size {
    int width;
    int height;
    
    Size(int w = 0, int h = 0) : width(w), height(h) {}
};

/**
 * Rectangle defined by position and size
 */
struct Rect {
    Point position;
    Size size;
    
    Rect(int x = 0, int y = 0, int w = 0, int h = 0)
        : position(x, y), size(w, h) {}
    
    Rect(const Point& pos, const Size& sz)
        : position(pos), size(sz) {}
    
    int left() const { return position.x; }
    int top() const { return position.y; }
    int right() const { return position.x + size.width; }
    int bottom() const { return position.y + size.height; }
    
    bool contains(const Point& p) const {
        return p.x >= left() && p.x < right() && 
               p.y >= top() && p.y < bottom();
    }
};

/**
 * Color pairs for ncurses
 */
enum class Color : uint8_t {
    DEFAULT = 0,
    BLACK = 1,
    RED = 2,
    GREEN = 3,
    YELLOW = 4,
    BLUE = 5,
    MAGENTA = 6,
    CYAN = 7,
    WHITE = 8
};

/**
 * Text attributes
 */
enum class Attribute : uint32_t {
    NORMAL = 0,
    BOLD = 1 << 0,
    DIM = 1 << 1,
    UNDERLINE = 1 << 2,
    REVERSE = 1 << 3,
    BLINK = 1 << 4
};

/**
 * Style combining color and attributes
 */
struct Style {
    Color foreground;
    Color background;
    uint32_t attributes;
    
    Style(Color fg = Color::DEFAULT, 
          Color bg = Color::DEFAULT, 
          uint32_t attr = static_cast<uint32_t>(Attribute::NORMAL))
        : foreground(fg), background(bg), attributes(attr) {}
    
    bool has_attribute(Attribute attr) const {
        return (attributes & static_cast<uint32_t>(attr)) != 0;
    }
};

/**
 * Input events
 */
enum class EventType {
    KEY_PRESS,
    MOUSE_CLICK,
    MOUSE_MOVE,
    FOCUS_IN,
    FOCUS_OUT,
    RESIZE
};

/**
 * Event structure
 */
struct Event {
    EventType type;
    int key;           // For KEY_PRESS
    Point position;    // For MOUSE events
    
    Event(EventType t = EventType::KEY_PRESS, int k = 0)
        : type(t), key(k), position(0, 0) {}
};

/**
 * Layout constraints
 */
struct Constraints {
    int min_width;
    int min_height;
    int max_width;
    int max_height;
    bool expand_horizontal;
    bool expand_vertical;
    
    Constraints()
        : min_width(0), min_height(0)
        , max_width(-1), max_height(-1)  // -1 = unlimited
        , expand_horizontal(false)
        , expand_vertical(false) {}
};

/**
 * Padding/margin
 */
struct Spacing {
    int left;
    int right;
    int top;
    int bottom;
    
    Spacing(int all = 0)
        : left(all), right(all), top(all), bottom(all) {}
    
    Spacing(int horizontal, int vertical)
        : left(horizontal), right(horizontal)
        , top(vertical), bottom(vertical) {}
    
    Spacing(int l, int r, int t, int b)
        : left(l), right(r), top(t), bottom(b) {}
};

} // namespace ui

#endif // UI_TYPES_H
