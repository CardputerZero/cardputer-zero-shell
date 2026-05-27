# Window Agent Task Backend

ZeroShell displays launcher and task UI. It does not own the compositor and it
does not infer tasks from launched child processes.

## Task Definition

A task is a compositor-managed toplevel/window reported by
`zero-window-agent`.

A task is not:

- a PID,
- a process group,
- a Shell child process,
- a desktop entry,
- a package,
- an APPLaunch app catalog row.

Desktop entries are used for launch metadata, labels, icons, ordering, and
matching. They do not prove that a task exists.

## Required Backend

ZeroShell must connect to:

```text
/run/user/$UID/cardputer-zero/window-agent.sock
```

It must use the `ZWA1` protocol documented by `cardputer-zero-os`.
`XDG_RUNTIME_DIR` is required for this socket path; ZeroShell must not use a
`/tmp` task-backend socket fallback.

The task panel, running badges, launch-pending resolution, and task activation
requests must all use this backend.

## Hard Constraints

- ZeroShell must not parse `wlrctl` output.
- ZeroShell must not invoke `wlrctl` for task focus, close, find, or list.
- ZeroShell must not scan `/proc` to infer tasks.
- ZeroShell must not treat launched child processes as tasks.
- ZeroShell must not use a `/tmp` socket as an alternate task backend.
- ZeroShell must not silently fall back to another task source.
- If the agent is offline, ZeroShell must display an explicit offline task
  backend state.

These constraints are part of the product contract, not temporary implementation
notes.

## Matching Apps To Tasks

ZeroShell receives task facts from the agent:

```text
id
app_id
title
activated
minimized
maximized
fullscreen
```

ZeroShell receives app metadata from APPLaunch desktop entries:

```text
Name
Exec
Icon
StartupWMClass
X-Zero-AppId
X-Zero-ShortName
X-Zero-Display
```

Running badges are computed by matching app metadata against task `app_id` and
`title`. The task itself still comes only from the agent.

## Failure UI

When the agent is unavailable, the task panel shows:

```text
WINDOW AGENT OFFLINE
```

Running badges are not guessed.

Launching apps remains allowed, but Shell cannot claim a running state until the
agent reports a matching compositor toplevel.

## Action Flow

Opening an app:

```text
Enter on app
  -> if matching agent task exists: activate task id
  -> else launch desktop Exec
  -> wait for agent to report matching task
```

Opening the task panel while ZeroShell is focused:

```text
4 or Tab
  -> show latest agent snapshot
```

Activating a task:

```text
Enter in task panel
  -> activate<TAB>task-id
```

No Shell task action may go through command-line compositor scraping.
