# Relationship To cardputer-zero-os

本文档专门记录 `cardputer-zero-shell` 和 `cardputer-zero-os` 的关系。

## Summary

```text
cardputer-zero-os
  prepares and authenticates the user session

cardputer-zero-shell
  runs inside that user session as the GUI desktop
```

## Flow

```text
Pi OS base system
  -> Raspberry Pi Imager creates user
  -> cardputer-zero-os userspace splash
  -> cardputer-zero-os GUI greeter
  -> PAM authenticates existing user
  -> cardputer-zero-session
  -> cardputer-zero-shell
```

## OS Responsibilities

`cardputer-zero-os` owns:

- startup splash,
- greeter,
- PAM configuration,
- existing user authentication,
- session launcher,
- device permissions,
- `zero-helper`,
- recovery mode,
- fallback when shell is missing.

## Shell Responsibilities

`cardputer-zero-shell` owns:

- launcher home,
- application scan,
- application launch,
- internal terminal page,
- small-screen controls,
- status display,
- power/display menu UI.

## Interface From OS To Shell

The OS starts:

```text
/opt/cardputer-zero-shell/bin/zero-shell
```

through:

```text
/usr/local/bin/cardputer-zero-session
```

The shell must already be installed before the session can enter the Zero desktop.

## Interface From Shell To OS

ZeroShell may call restricted helper actions:

```sh
/usr/local/sbin/zero-helper reboot
/usr/local/sbin/zero-helper shutdown
/usr/local/sbin/zero-helper display internal
/usr/local/sbin/zero-helper display mirror
/usr/local/sbin/zero-helper display extended
/usr/local/sbin/zero-helper network-restart
```

The exact allowed helper commands are defined by `cardputer-zero-os`. The helper
enters `pkexec`/polkit when root authorization is required.

ZeroShell should not assume arbitrary sudo rights or invoke `sudo` itself.

## HDMI Rule

ZeroShell is for the Cardputer Zero internal display.

It should not:

- disable LightDM,
- replace HDMI login,
- own the HDMI desktop session,
- force all displays through ZeroShell.

If HDMI is plugged in, the base OS / LightDM session can continue to exist independently.

## Wayland/labwc Graphics Rule

With the DRM/KMS + labwc graphics stack, the split becomes:

```text
cardputer-zero-os
  owns DRM/KMS setup, labwc session setup, seat/input policy, greeter/session

cardputer-zero-shell
  runs as a Wayland client and presents launcher/task UI
```

ZeroShell should not configure device-tree overlays, DRM cards, seats,
LightDM, or labwc startup policy. Those remain OS responsibilities.

## Recovery Rule

If ZeroShell fails or is missing, recovery belongs to `cardputer-zero-os`.

The shell should make failures visible, but it should not become a second login manager or recovery OS.
