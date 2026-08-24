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
// symlink it: ln -s ~/dotfiles/dsh-notify/notify.js ~/.dsh/profiles/web/dsh-desktop-notify.js)
//
// Then restart `dsh web`. No rebuild is needed; plugins load at boot.

import { spawn } from "node:child_process";

export const name = "desktop-notify";
export const inject = ["sessions"];

const APP_NAME = "dsh";

/** Fire a notify-send and forget. Never throws into the agent loop. */
function notify(summary, body) {
  try {
    const child = spawn("notify-send", ["-a", APP_NAME, summary, body], {
      detached: true,
      stdio: "ignore",
    });
    child.unref();
  } catch {
    // notification failure must never break DSH
  }
}

export function apply(ctx) {
  // Only ping "finished" for sessions the human actually drives, so
  // subagent / background turns don't spam. Armed on user/message, consumed
  // on the next completed turn/end.
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
    (sessionId, event) => {
      if (!event || typeof event.type !== "string") return;

      switch (event.type) {
        case "user/message":
          userDriven.add(sessionId);
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
          const kind = event.data?.reason?.kind;
          if (kind !== "completed") break;
          if (!userDriven.has(sessionId)) break; // subagent/autonomous turn
          if (!cooldownOk(sessionId)) break;
          userDriven.delete(sessionId);
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
}

export default apply;