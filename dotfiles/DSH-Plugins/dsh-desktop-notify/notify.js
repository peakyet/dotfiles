// dsh-desktop-notify — server-side plugin for the DSH web GUI
//
// Sends a desktop notification (notify-send → mako) when:
//   1. DSH asks the user a question (ask_user_question tool call), and
//   2. DSH finishes replying to a user message (turn/end, reason=completed).
//
// Loaded from the web profile (`~/.dsh/profiles/web/cordis.patch.yml`):
//
//   - insert:
//       - id: desktop-notify
//         name: './dsh-desktop-notify.js'   # file next to cordis.patch.yml
//         config: {}
//
// after putting this file there (e.g. keep the copy here in dotfiles and
// symlink it):
//
//   ln -s ~/dotfiles/dotfiles/DSH-Plugins/dsh-desktop-notify/notify.js \
//         ~/.dsh/profiles/web/dsh-desktop-notify.js
//
// Then restart `dsh web`. No rebuild is needed; plugins load at boot.

import { spawn } from "node:child_process";

export const name = "desktop-notify";

const APP_NAME = "dsh";

/** Fire a notify-send and forget. Never throws into the agent loop. */
function notify(summary, body) {
  try {
    const child = spawn(
      "notify-send",
      // `--` terminates option parsing so a body that starts with "-" is not
      // misread as a flag (the question text is user-supplied).
      ["-a", APP_NAME, "--", summary, body],
      { detached: true, stdio: "ignore" },
    );
    // A missing/broken notify-send surfaces as an async "error" event on the
    // child, not a throw. Swallow it: a failed spawn must never crash DSH.
    child.on("error", () => {});
    child.unref();
  } catch {
    // notification failure must never break DSH
  }
}

export function apply(ctx) {
  // Only ping "finished" for sessions the human actually drives, so
  // subagent / background turns don't spam. Armed on a real human user/message
  // (source.kind === "user"), consumed on the next turn/end regardless of how
  // that turn ended, so a later background completion can't fire a stale ping.
  const userDriven = new Set();
  // Per-session cooldown so rapid consecutive turns collapse into one ping.
  const lastFinished = new Map();
  const COOLDOWN_MS = 8_000;

  const cooldownOk = (sessionId) => {
    const last = lastFinished.get(sessionId);
    return last === void 0 || Date.now() - last > COOLDOWN_MS;
  };

  // { global: true } receives session/event for every session (the session
  // invariant plugin uses the same subscription pattern).
  ctx.on(
    "session/event",
    // The first argument is the live Session object, not its id — key all state
    // by session.id for stable identity and easy cleanup on disposal.
    (session, event) => {
      if (!session || !event || typeof event.type !== "string") return;
      const sessionId = session.id;

      switch (event.type) {
        case "user/message":
          // Only a direct human prompt counts. Synthetic agent.inject context,
          // schedule/tool-job notices, and goal-continuation rounds all carry a
          // non-"user" source and must not arm the "finished" ping.
          if (event.data?.source?.kind === "user") userDriven.add(sessionId);
          break;

        case "tool/call": {
          // ask_user_question tool invoked → DSH is waiting for a choice.
          if (event.data?.name === "ask_user_question") {
            let question;
            try {
              const args = JSON.parse(event.data.arguments);
              question = args?.questions?.[0]?.question;
            } catch {
              // arguments not JSON (defensive) — fall back to generic text
            }
            notify(
              "dsh needs your input",
              question ?? "A question is waiting for an answer.",
            );
          }
          break;
        }

        case "turn/end": {
          // Consume the arm for this session however the turn ended, so an
          // aborted/blocked turn or a cooldown-suppressed ping can never leave a
          // stale arm behind for a later background turn to release.
          const armed = userDriven.delete(sessionId);
          if (!armed) break;
          const kind = event.data?.reason?.kind;
          if (kind !== "completed") break;
          if (!cooldownOk(sessionId)) break;
          lastFinished.set(sessionId, Date.now());
          notify("dsh finished", "Your reply is ready.");
          break;
        }

        default:
          break;
      }
    },
    { global: true },
  );

  // Drop live session state when the session leaves the store, so userDriven /
  // lastFinished never grow unboundedly over the process lifetime.
  ctx.on(
    "session/disposed",
    (session) => {
      if (!session) return;
      userDriven.delete(session.id);
      lastFinished.delete(session.id);
    },
    { global: true },
  );
}

export default apply;
