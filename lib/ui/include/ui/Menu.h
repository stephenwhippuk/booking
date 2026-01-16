#ifndef UI_MENU_H
#define UI_MENU_H

#include "Widget.h"
#include "Window.h"
#include <string>
#include <vector>
#include <functional>
#include <memory>

namespace ui {

/**
 * MenuItem - Single item in a menu
 */
struct MenuItem {
    std::string text;
    std::string secondary_text;  // Optional secondary text (right-aligned)
    bool enabled;
    void* user_data;  // Optional custom data
    
    MenuItem(const std::string& text = "", 
             const std::string& secondary = "",
             bool enabled = true,
             void* data = nullptr)
        : text(text)
        , secondary_text(secondary)
        , enabled(enabled)
        , user_data(data) {}
};

/**
 * Menu - Navigable list of items with arrow key support
 * 
 * Displays a vertical list of items that can be navigated with arrow keys.
 * Supports scrolling, selection highlighting, and callbacks.
 */
class Menu : public Widget {
public:
    /**
     * Callback types
     */
    using SelectCallback = std::function<void(size_t index, const MenuItem& item)>;
    using ActivateCallback = std::function<void(size_t index, const MenuItem& item)>;
    
    /**
     * Constructor
     */
    Menu(int x, int y, int width, int height);
    
    /**
     * Constructor from Rect
     */
    explicit Menu(const Rect& bounds);
    
    /**
     * Destructor
     */
    virtual ~Menu() = default;
    
    /**
     * Add an item to the menu
     */
    void add_item(const MenuItem& item);
    
    /**
     * Add an item with just text
     */
    void add_item(const std::string& text);
    
    /**
     * Add an item with text and secondary text
     */
    void add_item(const std::string& text, const std::string& secondary_text);
    
    /**
     * Insert item at specific position
     */
    void insert_item(size_t index, const MenuItem& item);
    
    /**
     * Remove item at index
     */
    void remove_item(size_t index);
    
    /**
     * Clear all items
     */
    void clear_items();
    
    /**
     * Get all items
     */
    const std::vector<MenuItem>& get_items() const { return items_; }
    
    /**
     * Set all items at once
     */
    void set_items(const std::vector<MenuItem>& items);
    
    /**
     * Get number of items
     */
    size_t get_item_count() const { return items_.size(); }
    
    /**
     * Get item at index
     */
    const MenuItem& get_item(size_t index) const { return items_[index]; }
    
    /**
     * Update item at index
     */
    void set_item(size_t index, const MenuItem& item);
    
    /**
     * Get selected index
     */
    int get_selected_index() const { return selected_index_; }
    
    /**
     * Set selected index
     */
    void set_selected_index(int index);
    
    /**
     * Get the currently selected item (nullptr if none)
     */
    const MenuItem* get_selected_item() const;
    
    /**
     * Enable/disable borders
     */
    void set_bordered(bool bordered) { bordered_ = bordered; }
    
    bool is_bordered() const { return bordered_; }
    
    /**
     * Set title (shown in border if bordered)
     */
    void set_title(const std::string& title) { title_ = title; }
    
    std::string get_title() const { return title_; }
    
    /**
     * Enable/disable item numbering
     */
    void set_numbered(bool numbered) { numbered_ = numbered; }
    
    bool is_numbered() const { return numbered_; }
    
    /**
     * Set callback for when selection changes
     */
    void set_on_select(SelectCallback callback) {
        on_select_ = callback;
    }
    
    /**
     * Set callback for when Enter is pressed on an item
     */
    void set_on_activate(ActivateCallback callback) {
        on_activate_ = callback;
    }
    
    /**
     * Render the menu
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
     * Move selection up
     */
    void move_selection_up();
    
    /**
     * Move selection down
     */
    void move_selection_down();
    
    /**
     * Move selection to first item
     */
    void move_selection_home();
    
    /**
     * Move selection to last item
     */
    void move_selection_end();
    
    /**
     * Activate the currently selected item
     */
    void activate_selected();
    
    /**
     * Ensure selected item is visible (adjust scroll)
     */
    void ensure_selection_visible();
    
    /**
     * Get visible area height (content area inside border)
     */
    int get_visible_height() const;
    
    /**
     * Get content width (inside border)
     */
    int get_content_width() const;
    
    std::vector<MenuItem> items_;
    int selected_index_;
    int scroll_offset_;        // First visible item index
    bool bordered_;
    bool numbered_;
    std::string title_;
    
    SelectCallback on_select_;
    ActivateCallback on_activate_;
};

/**
 * Shared pointer type for menus
 */
using MenuPtr = std::shared_ptr<Menu>;

} // namespace ui

#endif // UI_MENU_H
