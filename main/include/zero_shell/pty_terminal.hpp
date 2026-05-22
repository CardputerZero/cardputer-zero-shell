#pragma once

#include "zero_shell/framebuffer_canvas.hpp"
#include "zero_shell/input_device.hpp"

#include <memory>
#include <string>

namespace zero_shell {

enum class TerminalPageResult {
    Minimized,
    Exited,
    Failed,
};

class TerminalSession {
public:
    TerminalSession(FramebufferCanvas &canvas,
                    InputDevice &input,
                    std::string command,
                    bool sysplause);
    ~TerminalSession();

    TerminalSession(const TerminalSession &) = delete;
    TerminalSession &operator=(const TerminalSession &) = delete;

    bool start();
    TerminalPageResult run_foreground();
    bool poll_background();
    bool running() const;
    int exit_code() const;
    const std::string &command() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

int run_terminal_page(FramebufferCanvas &canvas,
                      InputDevice &input,
                      const std::string &command,
                      bool sysplause);

} // namespace zero_shell
