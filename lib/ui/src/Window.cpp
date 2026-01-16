#include "ui/Window.h"
#include "ui/TextInput.h"
#include "ui/Menu.h"
#include <algorithm>

namespace ui {

Window::Window(int x, int y, int width, int height)
    : window_(nullptr)
    , bordered_(false)
{
    bounds_ = Rect(x, y, width, height);
    window_ = newwin(height, width, y, x);
    if (!window_) {
        // Handle error - could throw or set error flag
        window_ = stdscr;  // Fallback to standard screen
    }
}

Window::Window(const Rect& bounds)
    : Window(bounds.left(), bounds.top(), bounds.size.width, bounds.size.height)
{
}

Window::~Window() {
    if (window_ && window_ != stdscr) {
        delwin(window_);
        window_ = nullptr;
    }
}

Rect Window::get_content_rect() const {
    if (bordered_) {
        // Content area is inside the border
        return Rect(
            bounds_.left() + 1,
            bounds_.top() + 1,
            std::max(0, bounds_.size.width - 2),
            std::max(0, bounds_.size.height - 2)
        );
    }
    return bounds_;
}

void Window::clear() {
    if (window_) {
        wclear(window_);
    }
}

void Window::erase() {
    if (window_) {
        werase(window_);
    }
}

void Window::draw_border() {
    if (window_) {
        box(window_, 0, 0);
        
        // Draw title if present
        if (!title_.empty() && bounds_.size.width > 4) {
            int title_len = std::min(static_cast<int>(title_.size()), 
                                    bounds_.size.width - 4);
            std::string display_title = " " + title_.substr(0, title_len) + " ";
            mvwprintw(window_, 0, 2, "%s", display_title.c_str());
        }
    }
}

void Window::draw_text(int x, int y, const std::string& text) {
    if (window_) {
        mvwprintw(window_, y, x, "%s", text.c_str());
    }
}

void Window::draw_text(int x, int y, const std::string& text, const Style& style) {
    if (window_) {
        // Convert style to ncurses attributes
        int attr = 0;
        if (style.has_attribute(Attribute::BOLD)) attr |= A_BOLD;
        if (style.has_attribute(Attribute::DIM)) attr |= A_DIM;
        if (style.has_attribute(Attribute::UNDERLINE)) attr |= A_UNDERLINE;
        if (style.has_attribute(Attribute::REVERSE)) attr |= A_REVERSE;
        if (style.has_attribute(Attribute::BLINK)) attr |= A_BLINK;
        
        if (attr) wattron(window_, attr);
        mvwprintw(window_, y, x, "%s", text.c_str());
        if (attr) wattroff(window_, attr);
    }
}

void Window::draw_text_attr(int x, int y, const std::string& text, int attr) {
    if (window_) {
        if (attr) wattron(window_, attr);
        mvwprintw(window_, y, x, "%s", text.c_str());
        if (attr) wattroff(window_, attr);
    }
}

void Window::move_cursor(int x, int y) {
    if (window_) {
        wmove(window_, y, x);
    }
}

void Window::set_cursor_visible(int visibility) {
    curs_set(visibility);
}

void Window::set_scrollable(bool scrollable) {
    if (window_) {
        scrollok(window_, scrollable);
    }
}

void Window::add_child(WidgetPtr child) {
    if (!child) {
        return;
    }
    
    children_.push_back(child);
    
    // Auto-focus first focusable child
    if (!focused_child_ && child->can_focus()) {
        focus_child(child);
    }
}

void Window::remove_child(WidgetPtr child) {
    auto it = std::find(children_.begin(), children_.end(), child);
    if (it != children_.end()) {
        if (focused_child_ == child) {
            focused_child_.reset();
        }
        children_.erase(it);
    }
}

void Window::clear_children() {
    children_.clear();
    focused_child_.reset();
}

void Window::focus_child(WidgetPtr child) {
    if (!child || !child->can_focus()) {
        return;
    }
    
    // Unfocus previous child
    if (focused_child_) {
        focused_child_->set_focus(false);
    }
    
    // Focus new child
    focused_child_ = child;
    focused_child_->set_focus(true);
}

void Window::focus_next_child() {
    if (children_.empty()) {
        return;
    }
    
    // Find current focused child index
    int current_index = -1;
    for (size_t i = 0; i < children_.size(); ++i) {
        if (children_[i] == focused_child_) {
            current_index = i;
            break;
        }
    }
    
    // Find next focusable child
    for (size_t i = 1; i <= children_.size(); ++i) {
        size_t next_index = (current_index + i) % children_.size();
        if (children_[next_index]->can_focus()) {
            focus_child(children_[next_index]);
            return;
        }
    }
}

void Window::focus_previous_child() {
    if (children_.empty()) {
        return;
    }
    
    // Find current focused child index
    int current_index = -1;
    for (size_t i = 0; i < children_.size(); ++i) {
        if (children_[i] == focused_child_) {
            current_index = i;
            break;
        }
    }
    
    // Find previous focusable child
    for (size_t i = 1; i <= children_.size(); ++i) {
        size_t prev_index = (current_index - i + children_.size()) % children_.size();
        if (children_[prev_index]->can_focus()) {
            focus_child(children_[prev_index]);
            return;
        }
    }
}

void Window::render() {
    if (!visible_ || !window_) {
        return;
    }
    
    // Erase previous content
    erase();
    
    // Draw border if enabled
    if (bordered_) {
        draw_border();
    }
    
    // Render children
    render_children();
    
    // Stage window for update (double buffering)
    wnoutrefresh(window_);
}

void Window::refresh_now() {
    if (window_) {
        wrefresh(window_);
    }
}

void Window::set_bounds(const Rect& bounds) {
    if (bounds_.size.width != bounds.size.width || 
        bounds_.size.height != bounds.size.height) {
        bounds_ = bounds;
        recreate_window();
    } else {
        bounds_ = bounds;
        if (window_) {
            mvwin(window_, bounds.top(), bounds.left());
        }
    }
}

bool Window::handle_event(const Event& event) {
    // Propagate event to focused child first
    if (focused_child_ && focused_child_->handle_event(event)) {
        return true;
    }
    
    // Handle Tab for focus switching
    if (event.type == EventType::KEY_PRESS && event.key == '\t') {
        focus_next_child();
        return true;
    }
    
    return false;
}

void Window::on_resize() {
    recreate_window();
}

void Window::render_children() {
    if (!window_) {
        return;
    }
    
    // Render all visible children to this window
    for (auto& child : children_) {
        if (child && child->is_visible()) {
            // Try to cast to TextInput or Menu which have render(WINDOW*) methods
            auto textinput = std::dynamic_pointer_cast<TextInput>(child);
            if (textinput) {
                textinput->render(window_);
                continue;
            }
            
            auto menu = std::dynamic_pointer_cast<Menu>(child);
            if (menu) {
                menu->render(window_);
                continue;
            }
            
            // Fallback to regular render for other widget types
            child->render();
        }
    }
}

void Window::recreate_window() {
    // Delete old window
    if (window_ && window_ != stdscr) {
        delwin(window_);
    }
    
    // Create new window with new size
    window_ = newwin(bounds_.size.height, bounds_.size.width,
                    bounds_.top(), bounds_.left());
    
    if (!window_) {
        window_ = stdscr;  // Fallback
    }
}

} // namespace ui
