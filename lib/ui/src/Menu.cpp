#include "ui/Menu.h"
#include <algorithm>
#include <ncurses.h>

namespace ui {

Menu::Menu(int x, int y, int width, int height)
    : selected_index_(-1)
    , scroll_offset_(0)
    , bordered_(false)
    , numbered_(false)
{
    bounds_ = Rect(x, y, width, height);
    focusable_ = true;
}

Menu::Menu(const Rect& bounds)
    : Menu(bounds.left(), bounds.top(), bounds.size.width, bounds.size.height)
{
}

void Menu::add_item(const MenuItem& item) {
    items_.push_back(item);
    
    // Auto-select first enabled item
    if (selected_index_ < 0 && item.enabled) {
        selected_index_ = items_.size() - 1;
    }
}

void Menu::add_item(const std::string& text) {
    add_item(MenuItem(text));
}

void Menu::add_item(const std::string& text, const std::string& secondary_text) {
    add_item(MenuItem(text, secondary_text));
}

void Menu::insert_item(size_t index, const MenuItem& item) {
    if (index >= items_.size()) {
        add_item(item);
        return;
    }
    
    items_.insert(items_.begin() + index, item);
    
    // Adjust selection if needed
    if (selected_index_ >= static_cast<int>(index)) {
        selected_index_++;
    }
}

void Menu::remove_item(size_t index) {
    if (index >= items_.size()) {
        return;
    }
    
    items_.erase(items_.begin() + index);
    
    // Adjust selection
    if (selected_index_ >= static_cast<int>(items_.size())) {
        selected_index_ = items_.empty() ? -1 : items_.size() - 1;
    }
}

void Menu::clear_items() {
    items_.clear();
    selected_index_ = -1;
    scroll_offset_ = 0;
}

void Menu::set_items(const std::vector<MenuItem>& items) {
    items_ = items;
    selected_index_ = -1;
    scroll_offset_ = 0;
    
    // Auto-select first enabled item
    for (size_t i = 0; i < items_.size(); ++i) {
        if (items_[i].enabled) {
            selected_index_ = i;
            break;
        }
    }
}

void Menu::set_item(size_t index, const MenuItem& item) {
    if (index < items_.size()) {
        items_[index] = item;
    }
}

void Menu::set_selected_index(int index) {
    if (index < -1 || index >= static_cast<int>(items_.size())) {
        return;
    }
    
    // Skip disabled items
    if (index >= 0 && !items_[index].enabled) {
        return;
    }
    
    int old_index = selected_index_;
    selected_index_ = index;
    
    ensure_selection_visible();
    
    if (old_index != selected_index_ && on_select_ && selected_index_ >= 0) {
        on_select_(selected_index_, items_[selected_index_]);
    }
}

const MenuItem* Menu::get_selected_item() const {
    if (selected_index_ >= 0 && selected_index_ < static_cast<int>(items_.size())) {
        return &items_[selected_index_];
    }
    return nullptr;
}

void Menu::render() {
    if (!visible_) {
        return;
    }
    
    render(stdscr);
}

void Menu::render(WINDOW* parent_window) {
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
    int content_width = get_content_width();
    int visible_height = get_visible_height();
    
    // Draw items
    int visible_count = std::min(visible_height, static_cast<int>(items_.size()) - scroll_offset_);
    
    for (int i = 0; i < visible_count; ++i) {
        int item_index = scroll_offset_ + i;
        const MenuItem& item = items_[item_index];
        
        int item_y = content_y + i;
        
        // Clear line
        std::string blank(content_width, ' ');
        mvwprintw(parent_window, item_y, content_x, "%s", blank.c_str());
        
        // Determine style
        bool is_selected = (item_index == selected_index_);
        bool is_focused_selected = is_selected && focused_;
        
        if (is_focused_selected) {
            wattron(parent_window, A_REVERSE);
        } else if (is_selected) {
            wattron(parent_window, A_BOLD);
        } else if (!item.enabled) {
            wattron(parent_window, A_DIM);
        }
        
        // Draw item text
        int text_x = content_x;
        std::string prefix = "";
        
        if (numbered_) {
            prefix = std::to_string(item_index + 1) + ". ";
        } else if (is_selected) {
            prefix = "> ";
        } else {
            prefix = "  ";
        }
        
        int available_width = content_width - prefix.length();
        if (!item.secondary_text.empty()) {
            available_width -= item.secondary_text.length() + 1;
        }
        
        std::string display_text = item.text;
        if (static_cast<int>(display_text.length()) > available_width) {
            display_text = display_text.substr(0, available_width - 3) + "...";
        }
        
        mvwprintw(parent_window, item_y, text_x, "%s%s", prefix.c_str(), display_text.c_str());
        
        // Draw secondary text (right-aligned)
        if (!item.secondary_text.empty()) {
            int sec_x = content_x + content_width - item.secondary_text.length();
            mvwprintw(parent_window, item_y, sec_x, "%s", item.secondary_text.c_str());
        }
        
        if (is_focused_selected) {
            wattroff(parent_window, A_REVERSE);
        } else if (is_selected) {
            wattroff(parent_window, A_BOLD);
        } else if (!item.enabled) {
            wattroff(parent_window, A_DIM);
        }
    }
    
    // Draw scroll indicators if needed
    if (bordered_ && items_.size() > static_cast<size_t>(visible_height)) {
        if (scroll_offset_ > 0) {
            mvwprintw(parent_window, content_y - 1, x + width - 2, "^");
        }
        if (scroll_offset_ + visible_height < static_cast<int>(items_.size())) {
            mvwprintw(parent_window, content_y + visible_height, x + width - 2, "v");
        }
    }
}

bool Menu::handle_event(const Event& event) {
    if (!visible_ || !focused_ || event.type != EventType::KEY_PRESS) {
        return false;
    }
    
    int key = event.key;
    
    if (key == KEY_UP) {
        move_selection_up();
        return true;
    }
    
    if (key == KEY_DOWN) {
        move_selection_down();
        return true;
    }
    
    if (key == KEY_HOME) {
        move_selection_home();
        return true;
    }
    
    if (key == KEY_END) {
        move_selection_end();
        return true;
    }
    
    if (key == '\n' || key == KEY_ENTER) {
        activate_selected();
        return true;
    }
    
    // Number key selection (if numbered)
    if (numbered_ && key >= '1' && key <= '9') {
        int index = (key - '1');
        if (index < static_cast<int>(items_.size()) && items_[index].enabled) {
            set_selected_index(index);
            activate_selected();
            return true;
        }
    }
    
    return false;
}

Size Menu::get_preferred_size() const {
    int max_width = 0;
    for (const auto& item : items_) {
        int item_width = item.text.length();
        if (!item.secondary_text.empty()) {
            item_width += item.secondary_text.length() + 1;
        }
        max_width = std::max(max_width, item_width);
    }
    
    if (numbered_) {
        max_width += 4;  // Space for "99. "
    } else {
        max_width += 2;  // Space for "> "
    }
    
    if (bordered_) {
        max_width += 2;
    }
    
    int height = items_.size();
    if (bordered_) {
        height += 2;
    }
    
    return Size(std::max(max_width, bounds_.size.width), 
                std::max(height, bounds_.size.height));
}

void Menu::on_focus_gained() {
    // Nothing special needed
}

void Menu::on_focus_lost() {
    // Nothing special needed
}

void Menu::move_selection_up() {
    if (items_.empty() || selected_index_ < 0) {
        return;
    }
    
    // Find previous enabled item
    int new_index = selected_index_ - 1;
    while (new_index >= 0 && !items_[new_index].enabled) {
        new_index--;
    }
    
    if (new_index >= 0) {
        set_selected_index(new_index);
    }
}

void Menu::move_selection_down() {
    if (items_.empty()) {
        return;
    }
    
    // Find next enabled item
    int new_index = (selected_index_ < 0) ? 0 : selected_index_ + 1;
    while (new_index < static_cast<int>(items_.size()) && !items_[new_index].enabled) {
        new_index++;
    }
    
    if (new_index < static_cast<int>(items_.size())) {
        set_selected_index(new_index);
    }
}

void Menu::move_selection_home() {
    for (size_t i = 0; i < items_.size(); ++i) {
        if (items_[i].enabled) {
            set_selected_index(i);
            break;
        }
    }
}

void Menu::move_selection_end() {
    for (int i = items_.size() - 1; i >= 0; --i) {
        if (items_[i].enabled) {
            set_selected_index(i);
            break;
        }
    }
}

void Menu::activate_selected() {
    if (selected_index_ >= 0 && selected_index_ < static_cast<int>(items_.size())) {
        const MenuItem& item = items_[selected_index_];
        if (item.enabled && on_activate_) {
            on_activate_(selected_index_, item);
        }
    }
}

void Menu::ensure_selection_visible() {
    if (selected_index_ < 0) {
        return;
    }
    
    int visible_height = get_visible_height();
    
    // Scroll up if selected is above visible area
    if (selected_index_ < scroll_offset_) {
        scroll_offset_ = selected_index_;
    }
    
    // Scroll down if selected is below visible area
    if (selected_index_ >= scroll_offset_ + visible_height) {
        scroll_offset_ = selected_index_ - visible_height + 1;
    }
}

int Menu::get_visible_height() const {
    int height = bounds_.size.height;
    if (bordered_) {
        height -= 2;
    }
    return std::max(1, height);
}

int Menu::get_content_width() const {
    int width = bounds_.size.width;
    if (bordered_) {
        width -= 2;
    }
    return std::max(1, width);
}

} // namespace ui
