// The text-editing backend for Sagrado TextEdit: a line-array document with
// a caret and selection. Kept independent of Win32 and of the framebuffer so
// it can be swapped for a piece-table later without touching the renderer.
#pragma once
#include <algorithm>
#include <string>
#include <vector>

struct Pos {
    int line = 0, col = 0;
    bool operator==(const Pos &o) const { return line == o.line && col == o.col; }
    bool operator!=(const Pos &o) const { return !(*this == o); }
    bool operator<(const Pos &o) const {
        return line < o.line || (line == o.line && col < o.col);
    }
};

class TextDoc {
  public:
    TextDoc() : lines_{""} {}

    const std::vector<std::string> &lines() const { return lines_; }
    int line_count() const { return int(lines_.size()); }
    const std::string &line(int i) const { return lines_[size_t(i)]; }
    Pos caret() const { return caret_; }
    bool has_selection() const { return anchor_ != caret_; }

    // Selection range, ordered.
    Pos sel_lo() const { return anchor_ < caret_ ? anchor_ : caret_; }
    Pos sel_hi() const { return anchor_ < caret_ ? caret_ : anchor_; }

    void set_text(const std::string &text) {
        lines_.clear();
        std::string cur;
        for (char ch : text) {
            if (ch == '\r') continue;
            if (ch == '\n') {
                lines_.push_back(cur);
                cur.clear();
            } else {
                cur.push_back(ch);
            }
        }
        lines_.push_back(cur);
        caret_ = anchor_ = {0, 0};
    }

    std::string text() const {
        std::string out;
        for (size_t i = 0; i < lines_.size(); ++i) {
            out += lines_[i];
            if (i + 1 < lines_.size()) out.push_back('\n');
        }
        return out;
    }

    std::string selected_text() const {
        if (!has_selection()) return "";
        Pos a = sel_lo(), b = sel_hi();
        if (a.line == b.line)
            return lines_[size_t(a.line)].substr(a.col, b.col - a.col);
        std::string out = lines_[size_t(a.line)].substr(a.col);
        for (int i = a.line + 1; i < b.line; ++i) {
            out.push_back('\n');
            out += lines_[size_t(i)];
        }
        out.push_back('\n');
        out += lines_[size_t(b.line)].substr(0, b.col);
        return out;
    }

    // --- Editing ---------------------------------------------------------

    void insert_text(const std::string &s) {
        if (has_selection()) erase_selection();
        for (char ch : s) {
            if (ch == '\r') continue;
            if (ch == '\n')
                split_line();
            else
                lines_[size_t(caret_.line)].insert(
                    lines_[size_t(caret_.line)].begin() + caret_.col, ch);
            if (ch != '\n') caret_.col++;
        }
        anchor_ = caret_;
    }

    void type_char(char ch) { insert_text(std::string(1, ch)); }
    void newline() { insert_text("\n"); }

    void backspace() {
        if (has_selection()) {
            erase_selection();
            return;
        }
        if (caret_.col > 0) {
            lines_[size_t(caret_.line)].erase(caret_.col - 1, 1);
            caret_.col--;
        } else if (caret_.line > 0) {
            int prev = caret_.line - 1;
            int join = int(lines_[size_t(prev)].size());
            lines_[size_t(prev)] += lines_[size_t(caret_.line)];
            lines_.erase(lines_.begin() + caret_.line);
            caret_ = {prev, join};
        }
        anchor_ = caret_;
    }

    void del_forward() {
        if (has_selection()) {
            erase_selection();
            return;
        }
        std::string &ln = lines_[size_t(caret_.line)];
        if (caret_.col < int(ln.size())) {
            ln.erase(caret_.col, 1);
        } else if (caret_.line + 1 < int(lines_.size())) {
            ln += lines_[size_t(caret_.line + 1)];
            lines_.erase(lines_.begin() + caret_.line + 1);
        }
        anchor_ = caret_;
    }

    // --- Navigation (extend keeps the anchor for shift-selection) --------

    void move_left(bool extend) {
        if (caret_.col > 0)
            caret_.col--;
        else if (caret_.line > 0) {
            caret_.line--;
            caret_.col = int(lines_[size_t(caret_.line)].size());
        }
        after_move(extend);
    }
    void move_right(bool extend) {
        if (caret_.col < int(lines_[size_t(caret_.line)].size()))
            caret_.col++;
        else if (caret_.line + 1 < int(lines_.size())) {
            caret_.line++;
            caret_.col = 0;
        }
        after_move(extend);
    }
    void move_up(bool extend) {
        if (caret_.line > 0) {
            caret_.line--;
            clamp_col();
        } else {
            caret_.col = 0;
        }
        after_move(extend);
    }
    void move_down(bool extend) {
        if (caret_.line + 1 < int(lines_.size())) {
            caret_.line++;
            clamp_col();
        } else {
            caret_.col = int(lines_[size_t(caret_.line)].size());
        }
        after_move(extend);
    }
    void move_home(bool extend) {
        caret_.col = 0;
        after_move(extend);
    }
    void move_end(bool extend) {
        caret_.col = int(lines_[size_t(caret_.line)].size());
        after_move(extend);
    }

    void set_caret(Pos p, bool extend) {
        caret_ = clamp(p);
        after_move(extend);
    }
    void select_all() {
        anchor_ = {0, 0};
        caret_ = {int(lines_.size()) - 1,
                  int(lines_[lines_.size() - 1].size())};
    }

    Pos clamp(Pos p) const {
        if (p.line < 0) p.line = 0;
        if (p.line >= int(lines_.size())) p.line = int(lines_.size()) - 1;
        if (p.col < 0) p.col = 0;
        if (p.col > int(lines_[size_t(p.line)].size()))
            p.col = int(lines_[size_t(p.line)].size());
        return p;
    }

  private:
    void after_move(bool extend) {
        if (!extend) anchor_ = caret_;
    }
    void clamp_col() {
        int n = int(lines_[size_t(caret_.line)].size());
        if (caret_.col > n) caret_.col = n;
    }
    void split_line() {
        std::string &ln = lines_[size_t(caret_.line)];
        std::string tail = ln.substr(caret_.col);
        ln.erase(caret_.col);
        lines_.insert(lines_.begin() + caret_.line + 1, tail);
        caret_.line++;
        caret_.col = 0;
    }
    void erase_selection() {
        Pos a = sel_lo(), b = sel_hi();
        if (a.line == b.line) {
            lines_[size_t(a.line)].erase(a.col, b.col - a.col);
        } else {
            std::string head = lines_[size_t(a.line)].substr(0, a.col);
            std::string tail = lines_[size_t(b.line)].substr(b.col);
            lines_.erase(lines_.begin() + a.line + 1,
                         lines_.begin() + b.line + 1);
            lines_[size_t(a.line)] = head + tail;
        }
        caret_ = anchor_ = a;
    }

    std::vector<std::string> lines_;
    Pos caret_, anchor_;
};
