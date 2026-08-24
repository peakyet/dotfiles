# dsh-desktop-notify

A server-side plugin for the [DSH](https://github.com/deepseek-ai/deepseek-harness)
web GUI that sends a desktop notification when:

1. **DSH asks you a question** — the `ask_user_question` tool call fires
   `tool/call` with `name = "ask_user_question"`; the popup shows the first
   question text.
2. **DSH finishes a reply** — `turn/end` with `reason.kind = "completed"` in a
   session you sent a message to (subagent/autonomous turns are ignored, and a
   per-session cooldown collapses rapid turns into one ping).

Notifications go through `notify-send` (`libnotify`), so they appear in
mako / any notification daemon. With the mako config in this repo
(`dotfiles/mako/config`), they auto-dismiss after 5 s.

## Install

1. Make the plugin visible to the web profile (pick one):

   ```sh
   # A — recommended: symlink into the profile dir so the patch uses the
   #                    designed relative-specifier form
   ln -s ~/dotfiles/dotfiles/DSH-Plugins/notify.js ~/.dsh/profiles/web/dsh-desktop-notify.js

   # B — alternative: no symlink, reference across the home dir instead
   #     (works as long as `~/dotfiles` is at this path)
   #     name: '../../../dotfiles/dotfiles/DSH-Plugins/notify.js'
   ```

2. Append to `~/.dsh/profiles/web/cordis.patch.yml` (the profile's own patch
   layer — currently an empty `[]` block):

   ```yaml
   - insert:
       - id: desktop-notify
         name: './dsh-desktop-notify.js'
         config: {}
   ```

   With option B, use `name: '../../../dotfiles/dotfiles/DSH-Plugins/notify.js'` instead.

3. Restart `dsh web` (however you normally start it — Ctrl+C and relaunch, or
   `systemctl --user restart <service>`). Sessions persist; the page just
   reloads. No rebuild is needed — plugins load at boot.

## Verify

- Press a `Print`-style trigger or just ask DSH a question — you should get a
  mako popup with the question text.
- Ask DSH to do a task — when the reply finishes you get a "dsh finished"
  popup.

## Tune

- **Text**: edit the `notify("dsh needs your input", ...)` /
  `notify("dsh finished", ...)` calls in `notify.js`.
- **Cooldown**: `COOLDOWN_MS` in `notify.js` (8 s default) controls how often
  "finished" pings can fire per session.
- **Also notify on errors**: `turn/end` reasons other than `completed`
  (`error`, `blocked`, `aborted`, `interrupted`, `max-tokens`) are ignored;
  change `if (kind !== "completed") break;` to ping on them too.

## How it works

DSH's session events are published app-wide to `session/event` with
`{ global: true }` listeners (the session-invariant plugin subscribes the same
way). Event payloads (verified in `dsh-session`, `dsh-agent-loop`):

- `tool/call` → `data = { turn, step, callId, name, arguments }`
- `turn/end` → `data = { turn, reason: { kind } }`
- `user/message` → fired when the client appends your message

The plugin needs no DSH source changes and loads from plain JS at boot.