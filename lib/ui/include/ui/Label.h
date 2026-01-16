#ifndef UI_LABEL_H
#define UI_LABEL_H

#include "Widget.h"
#include <string>
#include <vector>
#include <ncurses.h>

namespace ui {

/**
 * Label - Static text display widget
 * 
 * Displays non-editable text with optional styling.
 * Supports single or multi-line text, alignment, and word wrapping.
 */
class Label : public Widget {
public:
    /**
     * Text alignment options
     */
    enum class Alignment {
        LEFT,
        CENTER,
        RIGHT
    };
    
    /**
     * Constructor - single line
     */
    Label(int x, int y, const std::string& text = "");
    
    /**
     * Constructor - bounded area for multi-line
     */
    Label(int x, int y, int width, int height, const std::string& text = "");
    
    /**
     * Constructor from Rect
     */
    explicit Label(const Rect& bounds, const std::string& text = "");
    
    /**
     * Destructor
     */
    virtual ~Label() = default;
    
    /**
     * Get the text
     */
    std::string get_text() const { return text_; }
    
    /**
     * Set the text
     */
    void set_text(const std::string& text);
    
    /**
     * Set text alignment
     */
    void set_alignment(Alignment alignment) {
        alignment_ = alignment;
    }
    
    Alignment get_alignment() const { return alignment_; }
    
    /**
     * Enable/disable word wrapping (multi-line only)
     */
    void set_word_wrap(bool wrap) {
        word_wrap_ = wrap;
    }
    
    bool is_word_wrap() const { return word_wrap_; }
    
    /**
     * Set text attributes (A_BOLD, A_DIM, etc.)
     */
    void set_attributes(int attr) {
        attributes_ = attr;
    }
    
    int get_attributes() const { return attributes_; }
    
    /**
     * Set foreground color (if color is supported)
     */
    void set_color_pair(int color_pair) {
        color_pair_ = color_pair;
    }
    
    int get_color_pair() const { return color_pair_; }
    
    /**
     * Render the label (to stdscr)
     */
    void render() override;
    
    /**
     * Render to specific window
     */
    void render(WINDOW* parent_window);
    
    /**
     * Get preferred size
     */
    Size get_preferred_size() const override;
    
private:
    std::string text_;
    Alignment alignment_;
    bool word_wrap_;
    int attributes_;
    int color_pair_;
    
    /**
     * Split text into lines for rendering
     */
    std::vector<std::string> get_lines() const;
    
    /**
     * Wrap text to fit width
     */
    std::vector<std::string> wrap_text(const std::string& text, int width) const;
};

/**
 * Shared pointer type for labels
 */
using LabelPtr = std::shared_ptr<Label>;

} // namespace ui

#endif // UI_LABEL_H
