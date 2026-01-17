#ifndef UI_LISTBOX_H
#define UI_LISTBOX_H

#include "Widget.h"
#include <string>
#include <vector>

namespace ui {

/**
 * ListBox - Simple list display widget
 * 
 * Displays a vertical list of items with optional border and title.
 * Read-only display, no selection or interaction.
 */
class ListBox : public Widget {
public:
    /**
     * Constructor
     */
    ListBox(int x, int y, int width, int height);
    
    /**
     * Constructor from Rect
     */
    ListBox(const Rect& bounds);
    
    /**
     * Destructor
     */
    virtual ~ListBox() = default;
    
    /**
     * Get items
     */
    const std::vector<std::string>& get_items() const { return items_; }
    
    /**
     * Set items
     */
    void set_items(const std::vector<std::string>& items);
    
    /**
     * Add item
     */
    void add_item(const std::string& item);
    
    /**
     * Clear all items
     */
    void clear();
    
    /**
     * Get/set bordered
     */
    bool is_bordered() const { return bordered_; }
    void set_bordered(bool bordered) { bordered_ = bordered; }
    
    /**
     * Get/set title
     */
    std::string get_title() const { return title_; }
    void set_title(const std::string& title) { title_ = title; }
    
    /**
     * Render the list box
     */
    virtual void render() override;
    
    /**
     * Render to specific window
     */
    void render(WINDOW* parent_window);
    
    /**
     * Handle events (ListBox doesn't handle any)
     */
    virtual bool handle_event(const Event& event) override { return false; }

private:
    std::vector<std::string> items_;
    bool bordered_;
    std::string title_;
    int scroll_offset_;
    
    int get_visible_height() const;
};

using ListBoxPtr = std::shared_ptr<ListBox>;

} // namespace ui

#endif // UI_LISTBOX_H
