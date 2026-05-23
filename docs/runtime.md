# Runtime Contract

This document defines how ZeroShell runs, which process identity it must use,
and how its two display backends relate to `cardputer-zero-os`.

## Launch Chain

The current Wayland/labwc production chain is:

```text
cardputer-zero-os
  -> greetd/PAM authenticates existing Linux user
  -> /usr/local/bin/cardputer-zero-session
  -> /usr/local/bin/cardputer-zero-labwc-session
  -> labwc on /dev/dri/cardputer-zero-internal
  -> /opt/cardputer-zero-shell/bin/zero-shell-wayland
```

The legacy direct-framebuffer compatibility chain is explicit only:

```text
CARDPUTER_ZERO_SESSION_MODE=framebuffer
  -> /opt/cardputer-zero-shell/bin/zero-shell
```

`zero-greeter`, PAM, greetd, labwc startup policy, and DRM device selection are
not part of ZeroShell. They belong to `cardputer-zero-os`.

## Binary Paths

Installed binaries:

```text
/opt/cardputer-zero-shell/bin/zero-shell-wayland
/opt/cardputer-zero-shell/bin/zero-shell
```

`zero-shell-wayland` is the launcher/task UI client used inside the
Wayland/labwc session. `zero-shell` is the legacy direct-framebuffer
implementation.

## User Identity

ZeroShell must run as the authenticated Linux user.

Check:

```sh
ps -eo user,pid,args | grep zero-shell
```

Expected Wayland/labwc process shape:

```text
pi      1234 /opt/cardputer-zero-shell/bin/zero-shell-wayland
```

Not acceptable:

```text
root    1234 /opt/cardputer-zero-shell/bin/zero-shell-wayland
root    1234 /opt/cardputer-zero-shell/bin/zero-shell
```

The framebuffer implementation refuses to run as root. The Wayland client should
also be treated as a normal user desktop client, never as a root service.

## Display Targets

### Wayland/labwc Display

Wayland/labwc display model:

```text
ZeroShell Wayland client
  -> labwc compositor
  -> /dev/dri/cardputer-zero-internal
  -> SPI-1 internal ST7789 display
```

In this model ZeroShell must not open `/dev/fb0` or `/dev/fb1`. Output
ownership, focus, activation, minimize, close, and stacking belong to labwc.

### Legacy Framebuffer Display

Legacy display model:

```text
ZeroShell
  -> direct framebuffer
```

Framebuffer selection:

1. `ZEROSHELL_FBDEV` if set,
2. `/proc/fb` entry whose name contains `st7789`,
3. `/sys/class/graphics/fb*/name` containing `st7789` or `panel-mipi-dbid`,
4. fallback `/dev/fb0`.

This mode is intentionally separate from HDMI and desktop display managers. It
is not the Wayland/labwc multitasking model.

## Input Target

In the Wayland/labwc session:

- normal keyboard events go to the focused Wayland client through labwc;
- `Tab` is bound by labwc to `zero-shell-control tasks`;
- short/long `Esc` is handled by `zero-key-policy` from `cardputer-zero-os`.

In legacy framebuffer mode, ZeroShell reads Linux evdev directly:

1. `ZEROSHELL_KEYBOARD_DEVICE` if set,
2. `/dev/input/by-path/*3f804000.i2c*event*`,
3. `/sys/class/input/event*/device/name` containing `keyboard`, `keypad`, or `tca8418`,
4. fallback `/dev/input/event0`.

## Environment Variables

Supported environment variables:

| Variable | Meaning |
| --- | --- |
| `ZEROSHELL_APPLICATIONS_DIR` | Override application scan directory for development. |
| `ZEROSHELL_APPLAUNCH_DIR` | Override APPLaunch data root for development. |
| `ZEROSHELL_FBDEV` | Override framebuffer device in legacy mode. |
| `ZEROSHELL_KEYBOARD_DEVICE` | Override keyboard evdev device in legacy mode. |
| `ZERO_SHELL_WAYLAND_DEBUG` | Enable extra key/debug logging in the Wayland client. |

Production defaults:

```text
ZEROSHELL_APPLICATIONS_DIR=/usr/share/APPLaunch/applications
ZEROSHELL_APPLAUNCH_DIR=/usr/share/APPLaunch
```

## Child Process Identity

Applications launched by ZeroShell inherit the same user identity as ZeroShell.

ZeroShell must not launch user applications as root.

If an application requires privileged actions, that app should use the
restricted helper contract provided by `cardputer-zero-os`, usually:

```text
pkexec /usr/local/sbin/zero-helper <allowed-action>
```

`zero-helper` owns the `pkexec`/polkit transition. ZeroShell and child apps stay
normal user processes and should not wrap helper calls in arbitrary `sudo`.

## Session Recovery

If `zero-shell-wayland` is missing or fails in the Wayland/labwc session, recovery belongs to
`cardputer-zero-os` through SSH or HDMI LightDM. The session should not silently
fall back to the framebuffer shell.

ZeroShell itself does not own login recovery.

## Development Runtime

For legacy framebuffer development, run as a normal user:

```sh
ZEROSHELL_APPLICATIONS_DIR=./applications ./build/zero-shell
```

For Wayland development, run inside a Wayland session:

```sh
ZEROSHELL_APPLICATIONS_DIR=./applications ./build/zero-shell-wayland
```

Do not use `sudo` for normal shell execution.
