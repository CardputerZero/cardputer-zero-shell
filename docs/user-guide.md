# User Guide

本文档描述最终用户如何使用 ZeroShell。

## Where ZeroShell Appears

ZeroShell 出现在用户登录成功之后。

正常流程：

```text
Raspberry Pi OS boots
  -> cardputer-zero-os greeter
  -> user logs in
  -> cardputer-zero-session
  -> ZeroShell home screen
```

ZeroShell 不显示在：

- firmware boot stage
- kernel boot stage
- GUI greeter before login
- HDMI LightDM login

## Home Screen

MVP home screen 是一个三卡片 carousel：

```text
left / center / right
```

中心槽位是当前选中的应用。按 `Enter` 会启动中心应用。

顶部状态栏显示：

- `ZeroShell`
- WiFi 状态
- battery 百分比
- time

底部显示基础操作提示。

## Controls

MVP controls:

| Key | Action |
| --- | --- |
| `Left` | Select previous app |
| `Right` | Select next app |
| `Enter` | Open selected app |
| `Tab` | Open running task menu |
| `R` | Reload `/usr/share/APPLaunch/applications` |
| `Esc` | Open power menu |
| `Q` | Quit shell, mainly for development/recovery |

## Launching Apps

ZeroShell launches apps from:

```text
/usr/share/APPLaunch/applications/*.desktop
```

Each `.desktop` file declares the app name, command, icon and launch mode.

Example:

```ini
[Desktop Entry]
Name=LoFiBox
TryExec=/usr/lib/lofibox/lofibox-applaunch
Exec=/usr/lib/lofibox/lofibox-applaunch
Terminal=false
Icon=share/images/lofibox.png
Sysplause=false
```

ZeroShell also watches the APPLaunch directory and reloads when package installs
or manual file changes update the directory metadata. Press `R` if you want to
force a reload immediately.

## Terminal Apps

If an entry has:

```ini
Terminal=true
```

ZeroShell opens an internal framebuffer PTY terminal page and runs the command there.
Pressing `Esc` on a terminal page minimizes it back to the ZeroShell home screen
instead of killing it. A minimized terminal app stays in the running task list
and its launcher icon shows a `RUN` badge.

Examples:

- `bash`
- `top`
- `htop`
- `nmtui`
- small command-line tools

The MVP terminal is intentionally minimal. It is enough for basic interaction, but it is not yet a full terminal emulator for every advanced curses application.

## Running Tasks

`Terminal=true` apps can be minimized and restored.

Controls:

| Key | Action |
| --- | --- |
| `Esc` inside terminal | Minimize terminal task |
| `Tab` on home | Open task menu |
| `Up` / `Down` | Select task |
| `Enter` | Restore selected task |
| `Esc` / `Tab` | Close task menu |

Apps with a running minimized terminal task show a `RUN` badge on their launcher
card. `Terminal=false` apps still run as foreground blocking apps and do not
enter the task list in the MVP.

## External Apps

If an entry has:

```ini
Terminal=false
```

ZeroShell runs the command as a blocking external app. When the command exits, ZeroShell returns to the home screen.

This mode is intended for applications that can take over the screen or run their own UI.

## Reloading Apps

Press:

```text
R
```

to rescan:

```text
/usr/share/APPLaunch/applications
```

ZeroShell also watches directory modification time during its main loop and reloads when `.desktop` files change.

## Power Menu

Press:

```text
Esc
```

to open the power menu.

MVP menu items:

- Cancel
- Reboot
- Shutdown

Privileged power actions are routed through:

```text
/usr/local/sbin/zero-helper
```

`zero-helper` asks `cardputer-zero-os`/polkit for authorization when required.
ZeroShell does not directly run `sudo reboot`, `sudo shutdown`, arbitrary
`systemctl`, or arbitrary shell commands for system actions.

## HDMI And Other Screens

ZeroShell is for the Cardputer Zero internal small screen.

It is not the HDMI login path. HDMI can continue to use the base OS display manager, such as LightDM, as configured by `cardputer-zero-os` and Raspberry Pi OS.

ZeroShell should not disable or replace HDMI login.
