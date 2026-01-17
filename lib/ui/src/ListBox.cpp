#include "ui/ListBox.h"
#include <algorithm>
#include <ncurses.h>

namespace ui {

ListBox::ListBox(int x, int y, int width, int height)
    : bordered_(false)
    , scroll_offset_(0)
{
    bounds_ = Rect(x, y, width, height);
    focusable_ = false;  // ListBox is not interactive
}

ListBox::ListBox(const Rect& bounds)
    : ListBox(bounds.left(), bounds.top(), bounds.size.width, bounds.size.height)
{
}

void ListBox::set_items(const std::vector<std::string>& items) {
    items_ = items;
    scroll_offset_ = 0;
}

void ListBox::add_item(const std::string& item) {
    items_.push_back(item);
}

void ListBox::clear() {
    items_.clear();
    scroll_offset_ = 0;
}

int ListBox::get_visible_height() const {
    int height = bounds_.size.height;
    if (bordered_) {
        height -= 2;  // Account for border
    }
    return std::max(0, height);
}

void ListBox::render() {
    if (!visible_) {
        return;
    }
    
    render(stdscr);
}

void ListBox::render(WINDOW* parent_window) {
    if (!visible_ || !parent_window) {
        return;
    }
    
    int x = bounds_.left();
    int y = bounds_.top();
    int width = bounds_.size.width;
    int height = bounds_.size.height;
    
    // Draw border if enabled
    if (bordered_) {
        for (int i = 0; i < height; ++i) {
            mvwprintw(parent_window, y + i, x, "|");
            mvwprintw(parent_window, y + i, x + width - 1, "|");
        }
        for (int i = 0; i < width; ++i) {
            mvwprintw(parent_window, y, x + i, "-");
            mvwprintw(parent_window, y + height - 1, x + i, "-");
        }
        mvwprintw(parent_window, y, x, "+");
        mvwprintw(parent_window, y, x + width - 1, "+");
        mvwprintw(parent_window, y + height - 1, x, "+");
        mvwprintw(parent_window, y + height - 1, x + width - 1, "+");
        
        // Draw title
        if (!title_.empty() && width > 4) {
            int title_len = std::min(static_cast<int>(title_.size()), width - 4);
            std::string display_title = " " + title_.substr(0, title_len) + " ";
            mvwprintw(parent_window, y, x + 2, "%s", display_title.c_str());
        }
    }
    
    // Calculate content area
    int content_x = x + (bordered_ ? 1 : 0);
    int content_y = y + (bordered_ ? 1 : 0);
    int content_width = width - (bordered_ ? 2 : 0);
    int visible_height = get_visible_height();
    
    // Draw items
    int visible_count = std::min(visible_height, static_cast<int>(items_.size()) - scroll_offset_);
    
    for (int i = 0; i < visible_count; ++i) {
        int item_index = scroll_offset_ + i;
        const std::string& item = items_[item_index];
        
        int item_y = content_y + i;
        
        // Clear line
        std::string blank(content_width, ' ');
        mvwprintw(parent_window, item_y, content_x, "%s", blank.c_str());
        
        // Draw item text (truncate if needed)
        std::string display_text = item;
        if (static_cast<int>(display_text.length()) > content_width) {
            display_text = display_text.substr(0, content_width - 3) + "...";
        }
        
        mvwprintw(parent_window, item_y, content_x, "%s", display_text.c_str());
    }
    
    // Clear remaining lines if list is shorter than visible area
    for (int i = visible_count; i < visible_height; ++i) {
        std::string blank(content_width, ' ');
        mvwprintw(parent_window, content_y + i, content_x, "%s", blank.c_str());
    }
}

} // namespace ui
