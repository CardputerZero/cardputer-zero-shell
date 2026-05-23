# Wayland Task Model

This document defines the Wayland/labwc task model for `cardputer-zero-shell`.

The Wayland/labwc task model is not a patch that adds a process-level
multitasker to the direct-framebuffer implementation. It moves the Zero
internal screen into the standard Linux graphics stack:

```text
Zero internal ST7789 display
  -> DRM/KMS output
  -> labwc / wlroots compositor
  -> Wayland or Xwayland application windows
  -> ZeroShell as launcher/task UI client
```

## Current MVP

The current MVP is a direct-framebuffer launcher:

```text
ZeroShell
  -> opens internal framebuffer
  -> scans /usr/share/APPLaunch/applications/*.desktop
  -> draws launcher UI
  -> launches selected command
```

In that model, `Terminal=false` applications are blocking foreground processes.
They are not real compositor tasks because there is no compositor-managed
window object. A process may draw directly to the framebuffer, and ZeroShell has
no standard way to hide that drawing while the process keeps running.

This MVP remains useful for bring-up and recovery, but it is not the long-term
model for minimize, restore, or task switching.

## Wayland/labwc Definition

In the Wayland/labwc session, ZeroShell becomes:

- a Wayland client,
- a launcher UI,
- an APPLaunch-compatible desktop-entry scanner,
- a task UI frontend,
- a requester of compositor actions.

ZeroShell stops being:

- the display owner,
- a framebuffer arbitrator,
- a process tree task manager,
- a global keyboard interceptor,
- a window manager.

The compositor owns:

- outputs,
- surfaces,
- focus,
- activation,
- minimize/iconify state,
- window close requests,
- window stacking.

## Task Identity

In the Wayland/labwc session, a task is a compositor-managed toplevel/window.

It is not primarily:

- a PID,
- a process group,
- a `fork()` result,
- a `Terminal=true` session,
- an APPLaunch `.desktop` file.

Those are supporting metadata. The task exists when an application creates a
window that labwc can manage.

Example:

```text
desktop entry
  -> launches app command
  -> app process creates Wayland/Xwayland toplevel
  -> compositor exposes toplevel metadata
  -> ZeroShell displays it as a task item
```

The `.desktop` entry remains the launch and display contract. Runtime task
state should be matched through compositor metadata such as:

- Wayland `app_id`,
- window title,
- desktop entry id,
- `StartupWMClass` for Xwayland/X11 compatibility,
- app launch scope metadata when available.

## Application Requirement

The Wayland/labwc session only gives standard task management to
compositor-managed apps.

Apps such as LoFiBox and AppStore must eventually run as one of:

- Wayland clients,
- Xwayland clients,
- SDL clients using the Wayland video backend,
- GTK/Qt clients,
- another toolkit that creates compositor-visible windows.

An app that directly opens `/dev/fb0` or another framebuffer can remain a
compatibility app, but it cannot be minimized, restored, focused, or stacked by
labwc in the standard way.

`zero-shell-wayland` must not launch framebuffer-only entries by default. A
direct-fb app that is started inside the labwc session can fight the compositor
for the same internal display and produce visible flicker. Windowed apps should
declare `X-Zero-Display=wayland` or `X-Zero-Display=xwayland`, or provide
`X-Zero-AppId` / `StartupWMClass` hints that prove the app creates a
compositor-visible window.

## APPLaunch Contract

ZeroShell continues to scan:

```text
/usr/share/APPLaunch/applications/*.desktop
```

The existing fields remain valid:

- `Name`
- `Exec`
- `Icon`
- `Terminal`
- `TryExec`
- `Sysplause`
- `X-Zero-ShortName`

The Wayland/labwc session can add optional window-matching fields without breaking
the APPLaunch contract:

```ini
StartupWMClass=lofibox
X-Zero-AppId=io.github.cardputerzero.lofibox
```

These fields are hints. ZeroShell must not hard-code application-specific
aliases or special cases.

## Launch Model

In the Wayland/labwc session, launching an app should be non-blocking:

```text
ZeroShell activates launcher command
  -> app starts in user session
  -> compositor observes new window
  -> ZeroShell returns to launcher/task UI
```

For reliable cleanup, app launchers should preferably run inside a systemd user
scope:

```text
systemd-run --user --scope --collect <app command>
```

The scope is not the task identity. It is cleanup and fallback-termination
metadata for the process tree behind a window.

## Task UI

The task UI should show compositor toplevels, not only children that ZeroShell
started.

Minimum task item data:

- app icon,
- display label,
- active/focused state,
- minimized/background state,
- close availability.

The running badge on launcher cards should be driven by compositor window state
when the Wayland/labwc session is active.

## Esc Policy

Short and long Esc are global device interaction policies, not ordinary
ZeroShell key handlers once Wayland apps are focused.

Desired behavior:

```text
short Esc
  -> minimize/iconify active window
  -> show or focus ZeroShell

long Esc
  -> request close active window
  -> if the app does not exit, terminate its user app scope
```

This policy must be implemented in one of:

- labwc key bindings/actions where sufficient,
- a small Zero input-policy daemon,
- a narrow labwc customization,
- another compositor-side integration.

ZeroShell can display and request actions, but it cannot reliably receive Esc
while another Wayland client owns keyboard focus.

## Standard Close Before Kill

Long Esc should use a standard-friendly order:

1. request window close,
2. allow the app to exit cleanly,
3. fallback to terminating the user app scope,
4. avoid guessing a single PID when the app has children.

This handles wrapper commands such as:

```text
sh -lc /usr/lib/lofibox/lofibox-applaunch
  -> /usr/lib/lofibox/lofibox_zero_device
```

without pretending that the wrapper PID is the task.

## Non-Goals

The Wayland/labwc task model does not mean:

- ZeroShell becomes a compositor,
- ZeroShell implements a custom Wayland server,
- ZeroShell manages arbitrary Linux processes as windows,
- direct-fb apps get real minimize/restore semantics automatically,
- `Terminal=true` becomes a business-level app category.

## Migration State

The current verified stack already has:

- the internal ST7789 screen exposed as a DRM/KMS output,
- a greetd-backed login/session handoff,
- labwc bound to the internal output,
- `zero-shell-wayland` running as a Wayland client,
- non-blocking app launch for entries that declare `X-Zero-Display=wayland` or
  `X-Zero-Display=xwayland`,
- task UI populated from compositor toplevel state through `wlrctl`,
- short/long Esc handled by the OS input policy.

Remaining work is mainly polish and standardization:

- migrate AppStore and LoFiBox away from direct framebuffer rendering,
- prefer stable Wayland app ids for task matching,
- replace `wlrctl` output parsing with a direct toplevel protocol or a small
  window-state agent,
- add close-then-kill fallback through user app scopes.

## Acceptance Criteria

The Wayland/labwc task model is not complete until:

- ZeroShell runs as a Wayland client and does not open `/dev/fb0` or `/dev/fb1`.
- launching a `.desktop` entry is non-blocking from ZeroShell's point of view.
- graphical apps create compositor-managed windows.
- the task UI is populated from compositor toplevel state, not from child PIDs.
- a running badge appears because a matching compositor window exists.
- `Tab` can activate an existing window.
- short Esc minimizes the active window and returns to ZeroShell.
- long Esc requests close on the active window before fallback termination.
- direct-fb compatibility apps are clearly marked as not having standard
  minimize/restore semantics.

These criteria deliberately avoid treating process-control behavior as a
substitute for real window management.
