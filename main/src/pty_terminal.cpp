#include "zero_shell/pty_terminal.hpp"

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <pty.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>
#include <utility>
#include <vector>

namespace zero_shell {
namespace {

constexpr Color kBg{0, 0, 0};
constexpr Color kFg{80, 255, 120};
constexpr Color kDim{68, 88, 74};
constexpr int kCharW = 6;
constexpr int kCharH = 8;

class TerminalBuffer {
public:
    TerminalBuffer(int cols, int rows)
        : cols_(std::max(1, cols)),
          rows_(std::max(1, rows)),
          cells_(static_cast<size_t>(cols_) * static_cast<size_t>(rows_), ' ')
    {
    }

    void put(char ch)
    {
        if (ch == '\r') {
            col_ = 0;
            return;
        }
        if (ch == '\n') {
            newline();
            return;
        }
        if (ch == '\b' || ch == 0x7f) {
            if (col_ > 0) {
                --col_;
                set(row_, col_, ' ');
            }
            return;
        }
        if (ch == '\t') {
            int spaces = 4 - (col_ % 4);
            while (spaces-- > 0) {
                put(' ');
            }
            return;
        }
        if (static_cast<unsigned char>(ch) < 0x20) {
            return;
        }

        set(row_, col_, ch);
        ++col_;
        if (col_ >= cols_) {
            newline();
        }
    }

    void clear()
    {
        std::fill(cells_.begin(), cells_.end(), ' ');
        row_ = 0;
        col_ = 0;
    }

    std::string line(int row) const
    {
        if (row < 0 || row >= rows_) {
            return {};
        }
        std::string out(cells_.begin() + row * cols_, cells_.begin() + (row + 1) * cols_);
        while (!out.empty() && out.back() == ' ') {
            out.pop_back();
        }
        return out;
    }

    int rows() const { return rows_; }

private:
    void set(int row, int col, char ch)
    {
        if (row < 0 || col < 0 || row >= rows_ || col >= cols_) {
            return;
        }
        cells_[static_cast<size_t>(row) * cols_ + col] = ch;
    }

    void newline()
    {
        col_ = 0;
        ++row_;
        if (row_ < rows_) {
            return;
        }
        for (int r = 1; r < rows_; ++r) {
            for (int c = 0; c < cols_; ++c) {
                cells_[static_cast<size_t>(r - 1) * cols_ + c] =
                    cells_[static_cast<size_t>(r) * cols_ + c];
            }
        }
        std::fill(cells_.begin() + (rows_ - 1) * cols_, cells_.end(), ' ');
        row_ = rows_ - 1;
    }

    int cols_ = 0;
    int rows_ = 0;
    int row_ = 0;
    int col_ = 0;
    std::vector<char> cells_;
};

void render_terminal(FramebufferCanvas &canvas, const TerminalBuffer &buffer, const std::string &title)
{
    canvas.clear(kBg);
    canvas.fill_rect(0, 0, canvas.width(), 16, {8, 14, 10});
    canvas.draw_text(4, 4, title.empty() ? "Terminal" : title, kFg, 1);
    canvas.draw_text(canvas.width() - 52, 4, "ESC min", kDim, 1);

    for (int row = 0; row < buffer.rows(); ++row) {
        canvas.draw_text(2, 20 + row * kCharH, buffer.line(row), kFg, 1);
    }
    canvas.present();
}

std::string key_to_terminal_sequence(const KeyEvent &event)
{
    if (!event.text.empty()) {
        return event.text;
    }

    switch (event.key) {
    case Key::Enter:
        return "\r";
    case Key::Backspace:
        return "\x7f";
    case Key::Tab:
        return "\t";
    case Key::Left:
        return "\x1b[D";
    case Key::Right:
        return "\x1b[C";
    case Key::Up:
        return "\x1b[A";
    case Key::Down:
        return "\x1b[B";
    default:
        return {};
    }
}

} // namespace

struct TerminalSession::Impl {
    Impl(FramebufferCanvas &canvas_ref,
         InputDevice &input_ref,
         std::string command_value,
         bool sysplause_value)
        : canvas(canvas_ref),
          input(input_ref),
          command(std::move(command_value)),
          sysplause(sysplause_value),
          cols(std::max(20, canvas.width() / kCharW)),
          rows(std::max(8, (canvas.height() - 22) / kCharH)),
          buffer(cols, rows)
    {
    }

    bool read_pending()
    {
        if (master < 0) {
            return false;
        }

        char data[512];
        bool changed = false;
        while (true) {
            ssize_t n = read(master, data, sizeof(data));
            if (n > 0) {
                for (ssize_t i = 0; i < n; ++i) {
                    buffer.put(data[i]);
                }
                changed = true;
            } else {
                break;
            }
        }
        return changed;
    }

    bool reap_if_done()
    {
        if (pid <= 0 || !active) {
            return false;
        }

        int status = 0;
        pid_t rc = waitpid(pid, &status, WNOHANG);
        if (rc != pid) {
            return false;
        }

        if (WIFEXITED(status)) {
            last_exit_code = WEXITSTATUS(status);
        } else if (WIFSIGNALED(status)) {
            last_exit_code = 128 + WTERMSIG(status);
        }
        active = false;
        pid = -1;
        return true;
    }

    FramebufferCanvas &canvas;
    InputDevice &input;
    std::string command;
    bool sysplause = false;
    int cols = 0;
    int rows = 0;
    TerminalBuffer buffer;
    int master = -1;
    pid_t pid = -1;
    bool active = false;
    int last_exit_code = -1;
};

TerminalSession::TerminalSession(FramebufferCanvas &canvas,
                                 InputDevice &input,
                                 std::string command,
                                 bool sysplause)
    : impl_(std::make_unique<Impl>(canvas, input, std::move(command), sysplause))
{
}

TerminalSession::~TerminalSession()
{
    if (!impl_) {
        return;
    }
    if (impl_->pid > 0 && impl_->active) {
        kill(impl_->pid, SIGTERM);
        waitpid(impl_->pid, nullptr, 0);
    }
    if (impl_->master >= 0) {
        close(impl_->master);
    }
}

bool TerminalSession::start()
{
    if (!impl_ || impl_->active || impl_->pid > 0) {
        return false;
    }

    winsize ws{};
    ws.ws_col = static_cast<unsigned short>(impl_->cols);
    ws.ws_row = static_cast<unsigned short>(impl_->rows);
    ws.ws_xpixel = static_cast<unsigned short>(impl_->canvas.width());
    ws.ws_ypixel = static_cast<unsigned short>(impl_->canvas.height());

    pid_t pid = forkpty(&impl_->master, nullptr, nullptr, &ws);
    if (pid < 0) {
        impl_->buffer.put('E'); impl_->buffer.put('r'); impl_->buffer.put('r');
        impl_->buffer.put('o'); impl_->buffer.put('r');
        render_terminal(impl_->canvas, impl_->buffer, "Terminal");
        return false;
    }

    if (pid == 0) {
        setenv("TERM", "vt100", 1);
        execlp("/bin/sh", "sh", "-lc", impl_->command.c_str(), static_cast<char *>(nullptr));
        _exit(127);
    }

    impl_->pid = pid;
    impl_->active = true;
    impl_->last_exit_code = -1;
    fcntl(impl_->master, F_SETFL, fcntl(impl_->master, F_GETFL, 0) | O_NONBLOCK);
    return true;
}

TerminalPageResult TerminalSession::run_foreground()
{
    if (!impl_ || !impl_->active) {
        return TerminalPageResult::Exited;
    }

    render_terminal(impl_->canvas, impl_->buffer, impl_->command);

    while (impl_->active) {
        bool changed = impl_->read_pending();
        changed = impl_->reap_if_done() || changed;

        KeyEvent event = impl_->input.poll_event(std::chrono::milliseconds(30));
        if (event.key == Key::Escape) {
            return TerminalPageResult::Minimized;
        } else {
            std::string seq = key_to_terminal_sequence(event);
            if (!seq.empty()) {
                ssize_t written = write(impl_->master, seq.data(), seq.size());
                (void)written;
            }
        }

        if (changed) {
            render_terminal(impl_->canvas, impl_->buffer, impl_->command);
        }
    }

    if (impl_->sysplause) {
        std::string msg = "\n[ZeroShell] exit " + std::to_string(impl_->last_exit_code) + ". Enter/ESC back.";
        for (char ch : msg) {
            impl_->buffer.put(ch);
        }
        render_terminal(impl_->canvas, impl_->buffer, impl_->command);
        while (true) {
            KeyEvent event = impl_->input.poll_event(std::chrono::milliseconds(100));
            if (event.key == Key::Enter || event.key == Key::Escape) {
                break;
            }
        }
    }

    return TerminalPageResult::Exited;
}

bool TerminalSession::poll_background()
{
    if (!impl_) {
        return false;
    }
    impl_->read_pending();
    impl_->reap_if_done();
    return impl_->active;
}

bool TerminalSession::running() const
{
    return impl_ && impl_->active;
}

int TerminalSession::exit_code() const
{
    return impl_ ? impl_->last_exit_code : -1;
}

const std::string &TerminalSession::command() const
{
    static const std::string empty;
    return impl_ ? impl_->command : empty;
}

int run_terminal_page(FramebufferCanvas &canvas,
                      InputDevice &input,
                      const std::string &command,
                      bool sysplause)
{
    TerminalSession session(canvas, input, command, sysplause);
    if (!session.start()) {
        return -1;
    }
    TerminalPageResult result = session.run_foreground();
    if (result == TerminalPageResult::Exited) {
        return session.exit_code();
    }
    return 0;
}

} // namespace zero_shell
