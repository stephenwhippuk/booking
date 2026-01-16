#include "ui/Label.h"
#include <algorithm>
#include <sstream>
#include <vector>

namespace ui {

Label::Label(int x, int y, const std::string& text)
    : text_(text)
    , alignment_(Alignment::LEFT)
    , word_wrap_(false)
    , attributes_(0)
    , color_pair_(0)
{
    // Auto-size to text length
    bounds_ = Rect(x, y, text.length(), 1);
    focusable_ = false;  // Labels are not focusable
}

Label::Label(int x, int y, int width, int height, const std::string& text)
    : text_(text)
    , alignment_(Alignment::LEFT)
    , word_wrap_(true)
    , attributes_(0)
    , color_pair_(0)
{
    bounds_ = Rect(x, y, width, height);
    focusable_ = false;
}

Label::Label(const Rect& bounds, const std::string& text)
    : text_(text)
    , alignment_(Alignment::LEFT)
    , word_wrap_(bounds.size.height > 1)
    , attributes_(0)
    , color_pair_(0)
{
    bounds_ = bounds;
    focusable_ = false;
}

void Label::set_text(const std::string& text) {
    text_ = text;
    
    // Auto-adjust width for single-line labels
    if (bounds_.size.height == 1 && !word_wrap_) {
        bounds_.size.width = text.length();
    }
}

void Label::render() {
    if (!visible_) {
        return;
    }
    
    render(stdscr);
}

void Label::render(WINDOW* parent_window) {
    if (!visible_ || !parent_window) {
        return;
    }
    
    int x = bounds_.left();
    int y = bounds_.top();
    int width = bounds_.size.width;
    int height = bounds_.size.height;
    
    // Apply attributes
    if (attributes_) {
        wattron(parent_window, attributes_);
    }
    if (color_pair_) {
        wattron(parent_window, COLOR_PAIR(color_pair_));
    }
    
    // Get lines to render
    std::vector<std::string> lines = get_lines();
    
    // Render each line
    int line_num = 0;
    for (const auto& line : lines) {
        if (line_num >= height) {
            break;  // Don't exceed height
        }
        
        int line_y = y + line_num;
        std::string display_line = line;
        
        // Truncate or pad based on width
        if (static_cast<int>(display_line.length()) > width) {
            display_line = display_line.substr(0, width);
        }
        
        // Calculate x position based on alignment
        int line_x = x;
        if (alignment_ == Alignment::CENTER) {
            int padding = (width - display_line.length()) / 2;
            line_x = x + std::max(0, padding);
        } else if (alignment_ == Alignment::RIGHT) {
            int padding = width - display_line.length();
            line_x = x + std::max(0, padding);
        }
        
        // Draw the text (mvwprintw clears to end of line)
        wmove(parent_window, line_y, x);
        wclrtoeol(parent_window);
        mvwprintw(parent_window, line_y, line_x, "%s", display_line.c_str());
        
        line_num++;
    }
    
    // Remove attributes
    if (attributes_) {
        wattroff(parent_window, attributes_);
    }
    if (color_pair_) {
        wattroff(parent_window, COLOR_PAIR(color_pair_));
    }
}

Size Label::get_preferred_size() const {
    if (word_wrap_) {
        // For multi-line, use current bounds
        return bounds_.size;
    } else {
        // For single-line, size to text
        return Size(text_.length(), 1);
    }
}

std::vector<std::string> Label::get_lines() const {
    if (word_wrap_ && bounds_.size.width > 0) {
        // Wrap text to fit width
        return wrap_text(text_, bounds_.size.width);
    } else {
        // Single line or multi-line without wrapping
        std::vector<std::string> lines;
        std::istringstream iss(text_);
        std::string line;
        while (std::getline(iss, line)) {
            lines.push_back(line);
        }
        
        if (lines.empty() && !text_.empty()) {
            lines.push_back(text_);
        }
        
        return lines;
    }
}

std::vector<std::string> Label::wrap_text(const std::string& text, int width) const {
    std::vector<std::string> lines;
    
    if (width <= 0) {
        return lines;
    }
    
    std::istringstream iss(text);
    std::string line;
    
    while (std::getline(iss, line)) {
        // Process each line (handle existing newlines)
        if (line.empty()) {
            lines.push_back("");
            continue;
        }
        
        // Word wrap this line
        std::string current_line;
        std::istringstream line_stream(line);
        std::string word;
        
        while (line_stream >> word) {
            // Check if adding this word would exceed width
            if (current_line.empty()) {
                current_line = word;
            } else if (static_cast<int>(current_line.length() + 1 + word.length()) <= width) {
                current_line += " " + word;
            } else {
                // Start a new line
                lines.push_back(current_line);
                current_line = word;
            }
        }
        
        if (!current_line.empty()) {
            lines.push_back(current_line);
        }
    }
    
    return lines;
}

} // namespace ui
