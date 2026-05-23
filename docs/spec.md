# ZeroShell Specification

本文档定义当前 ZeroShell 的实现规格。ZeroShell 现在有两个后端：

- Wayland/labwc 后端：`zero-shell-wayland`，当前设备路线；
- legacy direct-framebuffer 后端：`zero-shell`，兼容和恢复用途。

## Scope

Wayland/labwc scope:

- Wayland launcher/task UI client,
- APPLaunch-compatible desktop scanner,
- three-card carousel,
- keyboard navigation while ZeroShell is focused,
- non-blocking app launch for windowed Wayland/Xwayland apps,
- running task list populated from compositor-visible toplevels,
- running badge based on compositor task matching,
- status bar,
- app reload,
- root refusal.

Legacy framebuffer scope:

- framebuffer GUI launcher
- APPLaunch-compatible desktop scanner
- three-card carousel
- keyboard navigation
- internal PTY terminal page for `Terminal=true`
- running task list for minimized `Terminal=true` apps
- blocking app runner for `Terminal=false`
- status bar
- app reload
- root refusal

Out of scope:

- login UI
- PAM
- OS setup
- udev setup
- user creation
- package installation
- full XDG desktop spec
- multi-window compositor
- graphical app store
- full terminal emulator
- hard-coded built-in app pages
- custom compositor/window manager
- global Wayland hotkeys inside ZeroShell
- direct-fb app multitasking inside the Wayland/labwc session

Wayland/labwc note:

The Wayland/labwc task identity is a compositor-managed toplevel/window. It is not a
PID or a `Terminal=true` session. See [`wayland-task-model.md`](wayland-task-model.md).

## Architecture

Current source layout:

```text
main/include/zero_shell/
  app_catalog.hpp
  framebuffer_canvas.hpp
  image.hpp
  input_device.hpp
  process_runner.hpp
  pty_terminal.hpp
  shell_ui.hpp
  status.hpp

main/src/
  app_catalog.cpp
  framebuffer_canvas.cpp
  image.cpp
  input_device.cpp
  main.cpp
  process_runner.cpp
  pty_terminal.cpp
  shell_ui.cpp
  status.cpp
  zero_shell_wayland.cpp
```

## Modules

### AppCatalog

Responsible for:

- scanning the applications directory,
- parsing `.desktop` files,
- applying `TryExec`,
- de-duplicating by `Exec`,
- returning app entries to UI.

Not responsible for:

- installing apps,
- validating full XDG desktop spec,
- app store metadata,
- privilege checks.

### FramebufferCanvas

Responsible for:

- opening framebuffer device,
- drawing basic rectangles,
- drawing text with built-in bitmap font,
- drawing placeholder icon tiles,
- presenting pixels to framebuffer.

Not responsible for:

- HDMI display,
- compositor integration,
- image decoding in MVP,
- LVGL runtime.

### InputDevice

Responsible for:

- opening keyboard evdev,
- mapping navigation keys,
- mapping basic text input for terminal page.

Not responsible for:

- keyboard layout configuration,
- udev permissions,
- all IME/input methods,
- mouse/touch UI.

### ShellUi

Responsible for:

- legacy framebuffer main event loop,
- app carousel state,
- status refresh,
- app launching,
- power menu,
- generic label formatting from `.desktop` fields.

Not responsible for:

- authentication,
- user session creation,
- OS recovery,
- knowing app-specific business names or aliases.

### WaylandShell

Responsible for:

- connecting to the Wayland compositor,
- creating a fixed 320x170 xdg-shell toplevel,
- setting app_id `cardputer-zero-shell`,
- drawing the launcher through shared-memory buffers,
- scanning APPLaunch desktop entries,
- launching windowed apps non-blocking,
- reading compositor task state through `wlrctl`,
- showing the `RUNNING TASKS` panel,
- processing command-file requests from `zero-shell-control`.

Not responsible for:

- owning DRM/KMS devices,
- being a compositor,
- receiving global keys while another Wayland client is focused,
- minimizing/closing windows directly without compositor mediation,
- launching direct-framebuffer apps inside the Wayland/labwc session.

### PtyTerminal

Responsible for:

- creating a PTY with `forkpty`,
- running terminal commands,
- rendering simple terminal output,
- forwarding basic keyboard input,
- minimizing and restoring terminal sessions owned by ZeroShell.

Not responsible for:

- complete VT100/xterm emulation,
- high fidelity curses rendering,
- scrollback history,
- terminal multiplexing.

### ProcessRunner

Responsible for:

- running blocking shell commands,
- calling `zero-helper` for allowed privileged actions.

Not responsible for:

- arbitrary sudo,
- package installation,
- systemd control,
- process supervision.

### Status

Responsible for:

- current time,
- basic WiFi signal detection,
- basic battery capacity detection.

Not responsible for:

- complete power management,
- network configuration,
- battery calibration.

## UI Specification

Resolution target:

```text
320x170 class internal display
```

Home layout:

```text
top status bar
three-card carousel
mode/status hint
bottom control hint
```

Carousel:

```text
left / center / right
```

Center item is selected.

## Keyboard Specification

Wayland/labwc home:

| Key | Behavior |
| --- | --- |
| `Left` | Previous app |
| `Right` | Next app |
| `Enter` | Launch app |
| `Tab` | Task menu |
| `R` | Reload apps |

Wayland/labwc task menu:

| Key | Behavior |
| --- | --- |
| `Up` | Previous task |
| `Down` / `Left` / `Right` | Next task |
| `Enter` | Focus selected task through labwc |
| `Tab` / `Esc` | Close task menu |

Global Wayland/labwc app policy is owned by `cardputer-zero-os`:

| Key | Behavior |
| --- | --- |
| short `Esc` | Minimize active app window and focus ZeroShell |
| long `Esc` | Request close on active app window and focus ZeroShell |

Legacy framebuffer power menu:

| Key | Behavior |
| --- | --- |
| `Esc` | Power menu on home |

Legacy framebuffer terminal page:

| Key | Behavior |
| --- | --- |
| text keys | Forward text to PTY |
| `Enter` | `\r` |
| `Backspace` | DEL |
| arrow keys | ANSI cursor sequences |
| `Esc` | Minimize terminal task and return home |

In the Wayland/labwc session, launcher cards show a running badge when a
matching compositor window exists. In legacy framebuffer mode, launcher cards for apps with a
minimized running terminal task show a `RUN` badge. `Terminal=false` apps remain
foreground blocking processes in the legacy backend and do not enter that
backend's task list.

## Security And Privilege Specification

ZeroShell must:

- refuse to run as root,
- launch apps as the current user,
- call restricted `zero-helper` for privileged system actions,
- avoid arbitrary sudo/systemctl/apt wrappers.

ZeroShell must not:

- create users,
- store passwords,
- read password hashes,
- call PAM,
- grant itself device permissions,
- run the user desktop as root.

## Compatibility Specification

Stable compatibility surface:

```text
/usr/share/APPLaunch/applications/*.desktop
```

MVP-compatible fields:

- `Name`
- `Exec`
- `Icon`
- `Terminal`
- `TryExec`
- `Sysplause`
- `X-Zero-ShortName`
- `StartupWMClass`
- `X-Zero-AppId`
- `X-Zero-Display`

The Wayland/labwc shell requires `X-Zero-Display=wayland` or `X-Zero-Display=xwayland` before
launching an entry. This prevents a direct-fb app from fighting labwc for the
internal display.

## Current Implementation Note

The legacy framebuffer backend uses direct framebuffer drawing instead of
importing the old APPLaunch LVGL code.

Reason:

旧 APPLaunch 的 LVGL/SConstruct implementation is heavily coupled to SDK build logic, built-in pages, AppStore assumptions, RadioLib, fixed tools and historical paths. Importing it wholesale would reintroduce the boundary drift this repo is meant to remove.

The design keeps the application contract stable so alternate UI backends can
change rendering without changing `.desktop` semantics.
