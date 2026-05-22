#include "zero_shell/shell_ui.hpp"

#include "zero_shell/process_runner.hpp"
#include "zero_shell/pty_terminal.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <filesystem>
#include <sstream>
#include <thread>
#include <utility>

namespace zero_shell {
namespace {

constexpr Color kZeroBg{0xE9, 0xE4, 0xD5};
constexpr Color kPanel{0xF4, 0xF0, 0xE6};
constexpr Color kTaskButton{0xEF, 0xE8, 0xD9};
constexpr Color kTile{0xDC, 0xD5, 0xC3};
constexpr Color kTileActive{0xF6, 0xF0, 0xDF};
constexpr Color kIconWell{0xF8, 0xF4, 0xEA};
constexpr Color kLabelStrip{0xEE, 0xE6, 0xD5};
constexpr Color kInk{0x17, 0x17, 0x17};
constexpr Color kLine{0x2A, 0x2A, 0x2A};
constexpr Color kMuted{0x6E, 0x6A, 0x61};
constexpr Color kSoftLine{0xBB, 0xB1, 0x9E};
constexpr Color kGridDot{0xC9, 0xC1, 0xAE};
constexpr Color kAccent{0xE6, 0x6A, 0x2C};
constexpr Color kOk{0x3A, 0x7D, 0x44};
constexpr Color kWarn{0xB9, 0x4A, 0x2C};
constexpr Color kShadow{0xBD, 0xB5, 0xA4};

constexpr int kBarHeight = 20;

std::filesystem::file_time_type catalog_mtime(const std::filesystem::path &dir)
{
    if (!std::filesystem::is_directory(dir)) {
        return {};
    }

    std::filesystem::file_time_type latest = std::filesystem::last_write_time(dir);
    for (const auto &entry : std::filesystem::directory_iterator(dir)) {
        if (!entry.is_regular_file() || entry.path().extension() != ".desktop") {
            continue;
        }
        auto mtime = std::filesystem::last_write_time(entry.path());
        if (mtime > latest) {
            latest = mtime;
        }
    }
    return latest;
}

std::string uppercase_ascii(std::string text)
{
    for (char &ch : text) {
        ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
    }
    return text;
}

std::string display_ascii(std::string text)
{
    std::string output;
    output.reserve(text.size());
    bool last_was_space = false;

    for (unsigned char ch : text) {
        if (ch >= 0x80) {
            continue;
        }
        if (std::isalnum(ch)) {
            output.push_back(static_cast<char>(std::toupper(ch)));
            last_was_space = false;
        } else if ((ch == ' ' || ch == '-' || ch == '_') && !output.empty() && !last_was_space) {
            output.push_back(' ');
            last_was_space = true;
        }
    }

    while (!output.empty() && output.back() == ' ') {
        output.pop_back();
    }
    return output;
}

std::string fit_text(std::string text, size_t max_chars)
{
    if (text.size() <= max_chars) {
        return text;
    }
    return text.substr(0, max_chars);
}

std::string label_for(const AppEntry &app, size_t max_chars = 8)
{
    std::string label = app.short_name.empty() ? display_ascii(app.name) : display_ascii(app.short_name);
    if (label.empty()) {
        label = display_ascii(app.id);
    }
    return fit_text(uppercase_ascii(label), max_chars);
}

std::string image_cache_key(const AppEntry &app)
{
    return app.id + ":" + app.icon;
}

} // namespace

ShellUi::ShellUi(FramebufferCanvas &canvas, InputDevice &input, AppCatalog catalog)
    : canvas_(canvas), input_(input), catalog_(std::move(catalog))
{
}

int ShellUi::run()
{
    reload_apps();
    update_status_if_needed(true);
    render();

    while (running_) {
        cleanup_tasks();

        auto current_mtime = catalog_mtime(catalog_.applications_dir());
        if (current_mtime != last_catalog_mtime_) {
            reload_apps();
            render();
        }

        update_status_if_needed();
        Key key = input_.poll(std::chrono::milliseconds(100));
        if (key != Key::None) {
            handle_key(key);
            render();
        }
    }

    return 0;
}

void ShellUi::reload_apps()
{
    cleanup_tasks();
    apps_ = catalog_.load();
    if (current_index_ >= apps_.size()) {
        current_index_ = apps_.empty() ? 0 : apps_.size() - 1;
    }
    icon_cache_.clear();
    last_catalog_mtime_ = catalog_mtime(catalog_.applications_dir());
}

void ShellUi::render()
{
    render_shell_frame();
    if (apps_.empty()) {
        render_empty();
    } else {
        render_home();
    }
    if (task_popup_open_) {
        render_task_popup();
    }
    if (power_menu_ == PowerMenuState::Open) {
        render_power_menu();
    }
    canvas_.present();
}

void ShellUi::render_shell_frame()
{
    canvas_.clear(kZeroBg);

    for (int y = 28; y < 138; y += 8) {
        for (int x = 8; x < canvas_.width(); x += 8) {
            canvas_.fill_rect(x, y, 1, 1, kGridDot);
        }
    }

    render_topbar();
    render_bottombar();
}

void ShellUi::render_topbar()
{
    canvas_.fill_rect(0, 0, canvas_.width(), kBarHeight, kPanel);
    canvas_.draw_rect(0, 0, canvas_.width(), kBarHeight, kLine);

    canvas_.draw_text(6, 5, status_.time.empty() ? "--:--" : status_.time, kInk, 1);

    int x = canvas_.width() - 78;
    if (status_.wifi_connected) {
        int bars = std::clamp(status_.wifi_signal / 25 + 1, 1, 4);
        for (int i = 0; i < 4; ++i) {
            Color color = i < bars ? kInk : kSoftLine;
            canvas_.fill_rect(x + i * 4, 12 - i * 2, 3, 3 + i * 2, color);
        }
    } else {
        canvas_.draw_text(x, 5, "--", kMuted, 1);
    }

    std::string battery = status_.battery_valid
        ? std::to_string(std::clamp(status_.battery_percent, 0, 100)) + "%"
        : "--";
    canvas_.draw_text(x + 21, 5, battery, status_.battery_valid ? kInk : kMuted, 1);

    int bx = canvas_.width() - 29;
    int by = 5;
    canvas_.draw_rect(bx, by, 22, 9, kLine);
    canvas_.fill_rect(bx + 22, by + 3, 2, 3, kLine);
    if (status_.battery_valid) {
        int fill = std::clamp((status_.battery_percent * 18) / 100, 0, 18);
        canvas_.fill_rect(bx + 2, by + 2, fill, 5,
                          status_.battery_percent >= 25 ? kOk : kWarn);
    }
}

void ShellUi::render_home()
{
    const size_t count = apps_.size();
    const int xs[3] = {42, 119, 196};
    const int ys[3] = {50, 42, 50};
    const int slot_order[3] = {1, 0, 2};
    std::vector<size_t> rendered_indices;

    for (int order = 0; order < 3; ++order) {
        int slot = slot_order[order];
        int offset = slot - 1;
        size_t index = (current_index_ + count + offset) % count;
        if (std::find(rendered_indices.begin(), rendered_indices.end(), index) != rendered_indices.end()) {
            continue;
        }
        rendered_indices.push_back(index);
        render_app_card(xs[slot], ys[slot], apps_[index], slot == 1);
    }

    if (const AppEntry *app = current_app()) {
        canvas_.draw_text_centered(canvas_.width() / 2, 128, label_for(*app, 12), kInk, 1);
    }
}

void ShellUi::render_empty()
{
    canvas_.draw_text_centered(canvas_.width() / 2, 58, "NO APPS", kInk, 1);
    canvas_.draw_text_centered(canvas_.width() / 2, 78, "APPLaunch/applications", kMuted, 1);
    canvas_.draw_text_centered(canvas_.width() / 2, 104, "PRESS R", kAccent, 1);
}

void ShellUi::render_app_card(int x, int y, const AppEntry &app, bool selected)
{
    constexpr int card_w = 72;
    constexpr int card_h = 74;
    Color card = selected ? kTileActive : kTile;
    Color border = selected ? kInk : kMuted;

    canvas_.fill_rect(x + 2, y + 2, card_w, card_h, kShadow);
    canvas_.fill_rect(x, y, card_w, card_h, card);
    canvas_.draw_rect(x, y, card_w, card_h, border);
    if (selected) {
        canvas_.draw_rect(x + 1, y + 1, card_w - 2, card_h - 2, kInk);
    }

    int well_x = x + 16;
    int well_y = y + 9;
    canvas_.fill_rect(well_x, well_y, 40, 40, kIconWell);
    canvas_.draw_rect(well_x, well_y, 40, 40, kInk);

    bool drew_icon = false;
    std::string icon_path = resolve_applaunch_path(app.icon);
    if (!icon_path.empty()) {
        std::string key = image_cache_key(app);
        auto found = icon_cache_.find(key);
        if (found == icon_cache_.end()) {
            Image image;
            if (load_png_image(icon_path, image)) {
                found = icon_cache_.emplace(key, std::move(image)).first;
            }
        }
        if (found != icon_cache_.end() && !found->second.empty()) {
            canvas_.draw_image_fit(found->second, well_x + 4, well_y + 4, 32, 32);
            drew_icon = true;
        }
    }

    if (!drew_icon) {
        canvas_.draw_rect(well_x + 9, well_y + 10, 22, 18, kInk);
        canvas_.fill_rect(well_x + 11, well_y + 8, 10, 3, kIconWell);
        canvas_.draw_rect(well_x + 11, well_y + 8, 10, 5, kInk);
    }

    int strip_x = x + 6;
    int strip_y = y + 55;
    canvas_.fill_rect(strip_x, strip_y, card_w - 12, 13, kLabelStrip);
    canvas_.draw_rect(strip_x, strip_y, card_w - 12, 13, kSoftLine);
    canvas_.draw_text_centered(x + card_w / 2, strip_y + 3, label_for(app), kInk, 1);

    if (selected) {
        canvas_.fill_rect(x + 10, y + 70, card_w - 20, 3, kAccent);
    }

    if (is_app_running(app)) {
        render_running_badge(x + card_w - 25, y + 5);
    }
}

void ShellUi::render_running_badge(int x, int y)
{
    canvas_.fill_rect(x + 2, y + 2, 22, 11, kShadow);
    canvas_.fill_rect(x, y, 22, 11, kAccent);
    canvas_.draw_rect(x, y, 22, 11, kInk);
    canvas_.draw_text(x + 3, y + 3, "RUN", kInk, 1);
}

void ShellUi::render_bottombar()
{
    int y = canvas_.height() - kBarHeight;
    canvas_.fill_rect(0, y, canvas_.width(), kBarHeight, kPanel);
    canvas_.draw_rect(0, y, canvas_.width(), kBarHeight, kLine);

    constexpr int task_w = 82;
    canvas_.fill_rect(0, y, task_w, kBarHeight, task_popup_open_ ? kTileActive : kTaskButton);
    canvas_.draw_rect(0, y, task_w, kBarHeight, kLine);
    canvas_.draw_rect(6, y + 4, 22, 11, kInk);
    canvas_.draw_text(9, y + 6, "TAB", kInk, 1);
    canvas_.draw_text(34, y + 6, "TASK", kInk, 1);
    canvas_.fill_rect(70, y + 4, 11, 11, kAccent);
    canvas_.draw_rect(70, y + 4, 11, 11, kInk);
    canvas_.draw_text(73, y + 6, std::to_string(task_count()), kInk, 1);

    canvas_.draw_text(94, y + 6, "< >", kMuted, 1);
    canvas_.draw_text(118, y + 6, "SELECT", kMuted, 1);
    canvas_.draw_text(190, y + 6, "ENTER OPEN", kInk, 1);
    canvas_.draw_text(276, y + 6, "ESC", kMuted, 1);
}

void ShellUi::render_task_popup()
{
    int w = 146;
    int h = task_count() == 0 ? 38 : std::min(100, 20 + static_cast<int>(task_count()) * 17);
    int x = 0;
    int y = canvas_.height() - kBarHeight - h;

    canvas_.fill_rect(x + 3, y + 3, w, h, kShadow);
    canvas_.fill_rect(x, y, w, h, kPanel);
    canvas_.draw_rect(x, y, w, h, kLine);
    canvas_.fill_rect(x, y, w, 17, kInk);
    canvas_.draw_text(7, y + 5, "RUNNING TASKS", kPanel, 1);

    if (task_count() == 0) {
        canvas_.draw_text(8, y + 24, "NO TASKS", kMuted, 1);
        return;
    }

    for (size_t i = 0; i < tasks_.size(); ++i) {
        int item_y = y + 21 + static_cast<int>(i) * 17;
        bool selected = i == task_selection_;
        canvas_.fill_rect(x + 6, item_y, w - 12, 14, selected ? kAccent : kPanel);
        canvas_.draw_rect(x + 6, item_y, w - 12, 14, selected ? kInk : kSoftLine);
        canvas_.draw_text(x + 10, item_y + 3, selected ? ">" : " ", selected ? kInk : kMuted, 1);
        canvas_.draw_text(x + 21, item_y + 3, label_for(tasks_[i].app, 14),
                          selected ? kInk : kMuted, 1);
    }
}

void ShellUi::render_power_menu()
{
    int x = 72;
    int y = 43;
    int w = 176;
    int h = 90;

    canvas_.fill_rect(x + 3, y + 3, w, h, kShadow);
    canvas_.fill_rect(x, y, w, h, kPanel);
    canvas_.draw_rect(x, y, w, h, kLine);
    canvas_.fill_rect(x, y, w, 17, kInk);
    canvas_.draw_text_centered(x + w / 2, y + 5, "POWER", kPanel, 1);

    const char *items[] = {"CANCEL", "REBOOT", "SHUTDOWN"};
    for (int i = 0; i < 3; ++i) {
        int item_y = y + 26 + i * 18;
        bool selected = i == power_selection_;
        canvas_.fill_rect(x + 12, item_y, w - 24, 14,
                          selected ? (i == 2 ? kWarn : kAccent) : kPanel);
        canvas_.draw_rect(x + 12, item_y, w - 24, 14, selected ? kInk : kSoftLine);
        canvas_.draw_text_centered(x + w / 2, item_y + 3, items[i],
                                   selected ? kInk : kMuted, 1);
    }
}

void ShellUi::launch_current()
{
    const AppEntry *app = current_app();
    if (!app) {
        return;
    }

    render_shell_frame();
    canvas_.draw_text_centered(canvas_.width() / 2, 62, "LAUNCHING", kInk, 1);
    canvas_.draw_text_centered(canvas_.width() / 2, 84, label_for(*app, 14), kAccent, 1);
    canvas_.present();

    if (app->terminal) {
        launch_terminal_app(*app);
        return;
    }

    int rc = run_blocking_command(app->exec);

    update_status_if_needed(true);
    render_shell_frame();
    canvas_.draw_text_centered(canvas_.width() / 2, 68, "RETURNED", kInk, 1);
    canvas_.draw_text_centered(canvas_.width() / 2, 88, "EXIT " + std::to_string(rc), kMuted, 1);
    canvas_.present();
    std::this_thread::sleep_for(std::chrono::milliseconds(450));
}

void ShellUi::launch_terminal_app(const AppEntry &app)
{
    for (size_t i = 0; i < tasks_.size(); ++i) {
        if (tasks_[i].app.id == app.id) {
            restore_task(i);
            return;
        }
    }

    auto session = std::make_unique<TerminalSession>(canvas_, input_, app.exec, app.sysplause);
    if (!session->start()) {
        render_shell_frame();
        canvas_.draw_text_centered(canvas_.width() / 2, 68, "TERMINAL FAILED", kInk, 1);
        canvas_.present();
        std::this_thread::sleep_for(std::chrono::milliseconds(450));
        return;
    }

    RunningTask task{app, std::move(session)};
    tasks_.push_back(std::move(task));
    restore_task(tasks_.size() - 1);
}

void ShellUi::restore_task(size_t task_index)
{
    if (task_index >= tasks_.size()) {
        return;
    }

    TerminalPageResult result = tasks_[task_index].terminal->run_foreground();
    if (result == TerminalPageResult::Exited || !tasks_[task_index].terminal->running()) {
        tasks_.erase(tasks_.begin() + static_cast<std::ptrdiff_t>(task_index));
        if (task_selection_ >= tasks_.size()) {
            task_selection_ = tasks_.empty() ? 0 : tasks_.size() - 1;
        }
    } else {
        task_selection_ = task_index;
    }
}

void ShellUi::cleanup_tasks()
{
    for (auto it = tasks_.begin(); it != tasks_.end();) {
        if (!it->terminal || !it->terminal->poll_background()) {
            it = tasks_.erase(it);
        } else {
            ++it;
        }
    }
    if (task_selection_ >= tasks_.size()) {
        task_selection_ = tasks_.empty() ? 0 : tasks_.size() - 1;
    }
}

bool ShellUi::is_app_running(const AppEntry &app) const
{
    return std::any_of(tasks_.begin(), tasks_.end(), [&](const RunningTask &task) {
        return task.app.id == app.id;
    });
}

size_t ShellUi::task_count() const
{
    return tasks_.size();
}

void ShellUi::move_left()
{
    if (apps_.empty()) {
        return;
    }
    current_index_ = (current_index_ + apps_.size() - 1) % apps_.size();
}

void ShellUi::move_right()
{
    if (apps_.empty()) {
        return;
    }
    current_index_ = (current_index_ + 1) % apps_.size();
}

void ShellUi::handle_key(Key key)
{
    if (power_menu_ == PowerMenuState::Open) {
        if (key == Key::Escape) {
            power_menu_ = PowerMenuState::Hidden;
            return;
        }
        if (key == Key::Up) {
            power_selection_ = (power_selection_ + 2) % 3;
            return;
        }
        if (key == Key::Down || key == Key::Left || key == Key::Right) {
            power_selection_ = (power_selection_ + 1) % 3;
            return;
        }
        if (key == Key::Enter) {
            if (power_selection_ == 0) {
                power_menu_ = PowerMenuState::Hidden;
            } else if (power_selection_ == 1) {
                run_zero_helper("reboot");
            } else if (power_selection_ == 2) {
                run_zero_helper("shutdown");
            }
            return;
        }
        return;
    }

    if (task_popup_open_) {
        if (key == Key::Escape || key == Key::Tab) {
            task_popup_open_ = false;
            return;
        }
        if (key == Key::Up && task_count() > 0) {
            task_selection_ = (task_selection_ + task_count() - 1) % task_count();
            return;
        }
        if ((key == Key::Down || key == Key::Left || key == Key::Right) && task_count() > 0) {
            task_selection_ = (task_selection_ + 1) % task_count();
            return;
        }
        if (key == Key::Enter) {
            task_popup_open_ = false;
            restore_task(task_selection_);
            return;
        }
        if (key == Key::Backspace) {
            return;
        }
    }

    switch (key) {
    case Key::Left:
        move_left();
        break;
    case Key::Right:
        move_right();
        break;
    case Key::Enter:
        launch_current();
        break;
    case Key::R:
        reload_apps();
        break;
    case Key::Tab:
        task_popup_open_ = !task_popup_open_;
        break;
    case Key::Escape:
    case Key::Power:
        power_menu_ = PowerMenuState::Open;
        power_selection_ = 0;
        break;
    case Key::Q:
        running_ = false;
        break;
    default:
        break;
    }
}

void ShellUi::update_status_if_needed(bool force)
{
    auto now = std::chrono::steady_clock::now();
    if (!force && now - last_status_update_ < std::chrono::seconds(5)) {
        return;
    }
    status_ = read_status();
    last_status_update_ = now;
    if (!force) {
        render();
    }
}

const AppEntry *ShellUi::current_app() const
{
    if (apps_.empty()) {
        return nullptr;
    }
    return &apps_[current_index_];
}

} // namespace zero_shell
