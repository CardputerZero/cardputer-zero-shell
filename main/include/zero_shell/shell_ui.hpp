#pragma once

#include "zero_shell/app_catalog.hpp"
#include "zero_shell/framebuffer_canvas.hpp"
#include "zero_shell/input_device.hpp"
#include "zero_shell/pty_terminal.hpp"
#include "zero_shell/status.hpp"

#include <chrono>
#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace zero_shell {

class ShellUi {
public:
    ShellUi(FramebufferCanvas &canvas, InputDevice &input, AppCatalog catalog);

    int run();

private:
    enum class PowerMenuState {
        Hidden,
        Open,
    };

    void reload_apps();
    void render();
    void render_shell_frame();
    void render_home();
    void render_empty();
    void render_topbar();
    void render_bottombar();
    void render_app_card(int x, int y, const AppEntry &app, bool selected);
    void render_task_popup();
    void render_power_menu();
    void render_running_badge(int x, int y);
    void launch_current();
    void launch_terminal_app(const AppEntry &app);
    void restore_task(size_t task_index);
    void cleanup_tasks();
    bool is_app_running(const AppEntry &app) const;
    size_t task_count() const;
    void move_left();
    void move_right();
    void handle_key(Key key);
    void update_status_if_needed(bool force = false);
    const AppEntry *current_app() const;

    FramebufferCanvas &canvas_;
    InputDevice &input_;
    AppCatalog catalog_;
    std::vector<AppEntry> apps_;
    StatusSnapshot status_;
    std::chrono::steady_clock::time_point last_status_update_{};
    std::filesystem::file_time_type last_catalog_mtime_{};
    std::unordered_map<std::string, Image> icon_cache_;
    size_t current_index_ = 0;
    PowerMenuState power_menu_ = PowerMenuState::Hidden;
    bool task_popup_open_ = false;
    struct RunningTask {
        AppEntry app;
        std::unique_ptr<TerminalSession> terminal;
    };
    std::vector<RunningTask> tasks_;
    size_t task_selection_ = 0;
    int power_selection_ = 0;
    bool running_ = true;
};

} // namespace zero_shell
