# Relationship To cardputer-zero-os

`cardputer-zero-os` and `cardputer-zero-shell` are separate layers.

## Ownership

`cardputer-zero-os` owns:

- internal DRM/KMS display setup,
- greetd/PAM/logind login,
- labwc session startup,
- global key policy,
- polkit agent,
- `zero-helper`,
- device permissions,
- HDMI/LightDM recovery policy.

`cardputer-zero-shell` owns:

- APPLaunch scanning,
- launcher UI,
- non-blocking app launch,
- running badge,
- running task panel,
- task activation requests.

## Dependency Direction

```text
cardputer-zero-os
  -> starts cardputer-zero-shell-session inside the authenticated user session
  -> cardputer-zero-shell-session starts zero-window-agent
  -> cardputer-zero-shell-session execs /opt/cardputer-zero-shell/bin/zero-shell-wayland

cardputer-zero-shell
  -> may request controlled OS actions through zero-helper/polkit
```

The reverse direction is not allowed. Shell must not configure the OS, own
login, create users, or become a privileged service.

## Interface

OS starts the shell inside a real user Wayland session:

```text
/usr/local/bin/cardputer-zero-session
  -> /usr/local/bin/cardputer-zero-labwc-session
  -> labwc -S /usr/local/bin/cardputer-zero-shell-session
  -> zero-window-agent
  -> /opt/cardputer-zero-shell/bin/zero-shell-wayland
```

Shell expects:

- `WAYLAND_DISPLAY`,
- `XDG_RUNTIME_DIR`,
- `/usr/share/APPLaunch/applications`,
- `/usr/share/APPLaunch/share/images`,
- `zero-window-agent` at
  `/run/user/$UID/cardputer-zero/window-agent.sock` for task state and task
  control.

Global `Tab` and `Esc` behavior is delivered through:

```text
/usr/local/bin/zero-shell-control
```

`zero-shell-control` is also a client of `zero-window-agent`. It must not use
`wlrctl` or process-tree guessing as a production fallback.
