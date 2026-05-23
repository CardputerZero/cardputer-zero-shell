# User Guide

This guide describes the current Wayland/labwc user experience. The legacy
direct-framebuffer behavior is noted where it differs.

## Where ZeroShell Appears

ZeroShell appears after login:

```text
Raspberry Pi OS boots
  -> cardputer-zero-os internal greeter
  -> user logs in through greetd/PAM
  -> cardputer-zero-session
  -> labwc on the internal screen
  -> ZeroShell home screen
```

ZeroShell does not appear during:

- firmware boot,
- kernel boot,
- the GUI greeter before login,
- HDMI LightDM login.

## Home Screen

The home screen is a three-card carousel:

```text
left / center / right
```

The center slot is the selected app. Press `Enter` to launch it.

The top bar shows time and basic status. The bottom bar shows the most important
controls, including `Tab` for the running task panel.

## Wayland/labwc Controls

Home:

| Key | Action |
| --- | --- |
| `Left` | Select previous app |
| `Right` | Select next app |
| `Enter` | Open selected app |
| `Tab` | Toggle running task panel |
| `R` | Reload `/usr/share/APPLaunch/applications` |

Running task panel:

| Key | Action |
| --- | --- |
| `Up` / `Down` | Select task |
| `Enter` | Focus selected task |
| `Tab` / `Esc` | Hide task panel |

Global app policy:

| Key | Action |
| --- | --- |
| Short `Esc` | Minimize the active app window and return to ZeroShell |
| Long `Esc` | Request close on the active app window and return to ZeroShell |

Short/long Esc is implemented by `cardputer-zero-os` because a normal Wayland
client cannot receive global keys while another app has focus.

## Launching Apps

ZeroShell launches apps from:

```text
/usr/share/APPLaunch/applications/*.desktop
```

Example Wayland/labwc entry:

```ini
[Desktop Entry]
Name=LoFiBox
TryExec=lofibox
Exec=lofibox
Terminal=false
Icon=share/images/lofibox.png
Sysplause=false
X-Zero-Display=xwayland
StartupWMClass=lofibox
```

The Wayland/labwc shell only launches entries that declare a compositor-managed display mode:

```ini
X-Zero-Display=wayland
```

or:

```ini
X-Zero-Display=xwayland
```

Direct framebuffer apps should not be launched inside the labwc session by
default because they can fight the compositor for the internal screen.

## Running Tasks

In the Wayland/labwc session, running tasks are windows that labwc can see.

A running task is not simply a PID or a child process. For example, an app may
start through a shell wrapper and then create a separate GUI process. ZeroShell
therefore uses compositor-visible toplevels instead of treating process trees as
the task list.

If an app is visible on screen but absent from the running task panel, check:

```sh
XDG_RUNTIME_DIR=/run/user/1000 WAYLAND_DISPLAY=wayland-0 wlrctl toplevel list
```

If it does not appear there, it is probably not a Wayland/Xwayland window.

## Reloading Apps

Press:

```text
R
```

to rescan:

```text
/usr/share/APPLaunch/applications
```

ZeroShell also watches directory modification time during its main loop and
reloads when `.desktop` files change.

## Privileged Actions

ZeroShell and child apps run as normal users. Privileged actions should go
through:

```text
pkexec /usr/local/sbin/zero-helper <allowed-action>
```

`zero-helper` and the polkit agent belong to `cardputer-zero-os`. ZeroShell does
not directly run arbitrary `sudo`, `systemctl`, or `apt` commands.

## Legacy Framebuffer Notes

The legacy `zero-shell` binary still supports:

- direct framebuffer drawing,
- internal evdev keyboard handling,
- `Terminal=true` entries through a minimal PTY terminal page,
- a power menu.

That backend is useful for compatibility and recovery, but it is not the Wayland/labwc
multitasking model. In legacy mode, `Terminal=false` apps are blocking
foreground processes and do not become compositor-managed running tasks.

## HDMI And Other Screens

ZeroShell is for the Cardputer Zero internal small screen.

It is not the HDMI login surface. HDMI can continue to use the base OS display
manager, such as LightDM, as configured by `cardputer-zero-os` and Raspberry Pi
OS.

ZeroShell should not disable or replace HDMI login.
