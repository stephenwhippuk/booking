#include "ui/MessageBox.h"
#include <algorithm>
#include <sstream>
#include <cstring>

namespace ui {

MessageBox::MessageBox(int width, int height, const std::string& title, const std::string& message)
    : Widget()
    , title_(title)
    , message_(message)
    , width_(width)
    , height_(height)
    , visible_(false)
{
}

void MessageBox::render() {
    // Render to stdscr by default
    render(stdscr);
}

void MessageBox::render(WINDOW* parent) {
    if (!visible_) return;
    
    int max_y, max_x;
    getmaxyx(parent, max_y, max_x);
    
    // Center the message box
    int start_y = (max_y - height_) / 2;
    int start_x = (max_x - width_) / 2;
    
    // Create window for message box
    WINDOW* box_win = derwin(parent, height_, width_, start_y, start_x);
    if (!box_win) return;
    
    // Draw border with title
    box(box_win, 0, 0);
    
    // Draw title centered at top
    int title_x = (width_ - title_.length() - 4) / 2;
    mvwprintw(box_win, 0, title_x, "[ %s ]", title_.c_str());
    
    // Wrap and display message
    std::vector<std::string> lines = wrap_text(message_, width_ - 4);
    int message_start_y = (height_ - lines.size() - 2) / 2;
    
    for (size_t i = 0; i < lines.size() && i < (size_t)(height_ - 4); i++) {
        int line_x = (width_ - lines[i].length()) / 2;
        mvwprintw(box_win, message_start_y + i + 1, line_x, "%s", lines[i].c_str());
    }
    
    // Draw instruction at bottom
    const char* instruction = "Press Enter to close";
    int instr_x = (width_ - strlen(instruction)) / 2;
    wattron(box_win, A_DIM);
    mvwprintw(box_win, height_ - 2, instr_x, "%s", instruction);
    wattroff(box_win, A_DIM);
    
    wrefresh(box_win);
    delwin(box_win);
}

bool MessageBox::handle_event(const Event& event) {
    if (!visible_) return false;
    
    if (event.type == EventType::KEY_PRESS) {
        int ch = event.key;
        if (ch == '\n' || ch == '\r' || ch == KEY_ENTER) {
            visible_ = false;
            if (on_close_) {
                on_close_();
            }
            return true;
        }
    }
    
    return true;  // Consume all input when visible (modal behavior)
}

void MessageBox::set_message(const std::string& message) {
    message_ = message;
}

void MessageBox::set_title(const std::string& title) {
    title_ = title;
}

void MessageBox::set_on_close(std::function<void()> callback) {
    on_close_ = callback;
}

std::vector<std::string> MessageBox::wrap_text(const std::string& text, int max_width) {
    std::vector<std::string> lines;
    std::istringstream stream(text);
    std::string word;
    std::string current_line;
    
    while (stream >> word) {
        if (current_line.empty()) {
            current_line = word;
        } else if (current_line.length() + 1 + word.length() <= (size_t)max_width) {
            current_line += " " + word;
        } else {
            lines.push_back(current_line);
            current_line = word;
        }
    }
    
    if (!current_line.empty()) {
        lines.push_back(current_line);
    }
    
    return lines;
}

} // namespace ui
