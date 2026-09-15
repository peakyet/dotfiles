#!/usr/bin/env bash
# Claude Code status line.
#
# Renders: model effort · dir · branch · ±lines · status · permission ·
#          total in · cached % · out · fast mode
#
# Token segments are CUMULATIVE FOR THE SESSION: total input (fresh + cached),
# the cached share of that input, and output. The payload has no session
# totals — context_window.* describes only the current window — so these are
# summed from the transcript, incrementally (see the counter block below).
# Reorder or drop segments by editing the `parts+=` block.

set -o pipefail

input=$(cat)

# ---- fields (single jq pass; one value per line so empty fields survive) ----
mapfile -t F < <(
  printf '%s' "$input" | jq -r '[
    (.model.display_name // "unknown" | gsub("[\\n\\t]"; " ")),
    (.effort.level // "" | gsub("[\\n\\t]"; " ")),
    (.workspace.current_dir // .cwd // "" | gsub("[\\n\\t]"; " ")),
    (.cost.total_lines_added // 0),
    (.cost.total_lines_removed // 0),
    (if .fast_mode then "on" else "off" end),
    (.permission_mode // ""),
    (.transcript_path // ""),
    (.session_id // "")
  ] | .[]' 2>/dev/null
)
model=${F[0]:-unknown}
effort=${F[1]:-}
curdir=${F[2]:-}
added=${F[3]:-0}
removed=${F[4]:-0}
fast=${F[5]:-off}
permjson=${F[6]:-}
tpath=${F[7]:-}
session=${F[8]:-}

curdir=${curdir:-$PWD}
if [ "$curdir" = "$HOME" ]; then
  dname='~'
else
  dname=$(basename "$curdir")
fi

branch=$(git -C "$curdir" branch --show-current 2>/dev/null)
[ -z "$branch" ] && branch=$(git -C "$curdir" rev-parse --short HEAD 2>/dev/null)

# ---- transcript-derived fields ---------------------------------------------
# The transcript keeps tiny state records ("permission-mode", ...) and the
# message log. Everything here reads only the tail, or the bytes appended since
# the last run, so it stays fast on big files.
status='Ready'
fresh=0
cached=0
outtok=0

if [ -n "$tpath" ] && [ -f "$tpath" ]; then
  tail1=$(tail -c 1048576 "$tpath" 2>/dev/null)

  # The turn stays open while Claude is generating or a tool is running: the
  # newest message record then ends with stop_reason "tool_use" (or is a user
  # record, e.g. a tool result). A terminal stop reason closes it.
  lastmsg=$(printf '%s\n' "$tail1" | tac | grep -m1 -E '"type":"(user|assistant)"')
  if [ -n "$lastmsg" ]; then
    status=$(printf '%s' "$lastmsg" | jq -r 'if .type == "assistant"
      and ((.message.stop_reason // "") | length > 0)
      and .message.stop_reason != "tool_use"
      then "Ready" else "Working" end' 2>/dev/null)
    [ -n "$status" ] || status='Working'
  fi

  # Live permission mode: prefer the status JSON, else the newest state record.
  if [ -z "$permjson" ]; then
    permjson=$(printf '%s\n' "$tail1" | tac | grep -m1 '"type":"permission-mode"' | sed -n 's/.*"permissionMode":"\([a-zA-Z]*\)".*/\1/p')
  fi

  # Cumulative session tokens. Counters are cached per session in
  # <byte-offset> <fresh> <cached> <out> form; only bytes appended since the
  # last run are summed.
  if [ -n "$session" ]; then
    state="${TMPDIR:-/tmp}/claude-session-tokens-${session}"
    size=$(stat -c %s "$tpath" 2>/dev/null || echo 0)
    off=0
    if [ -f "$state" ]; then
      read -r o a b c extra < "$state" 2>/dev/null
      if [ -n "$o" ] && [ -n "$a" ] && [ -n "$b" ] && [ -n "$c" ] && [ -z "$extra" ]; then
        case "$o$a$b$c" in
          *[!0-9]*) ;;                          # unusable state: recount from zero
          *) off=$o; fresh=$a; cached=$b; outtok=$c ;;
        esac
      fi
    fi
    # A shrinking transcript means it was rewritten (e.g. compaction): recount.
    [ "$size" -lt "$off" ] && { off=0; fresh=0; cached=0; outtok=0; }
    if [ "$size" -gt "$off" ]; then
      mapfile -t ADD < <(tail -c "+$((off + 1))" "$tpath" 2>/dev/null | jq -s -r '
        [.[] | select(.type == "assistant") | .message.usage] as $u
        | ($u | [.[] | (.input_tokens // 0)] | add // 0),
          ($u | [.[] | ((.cache_read_input_tokens // 0) + (.cache_creation_input_tokens // 0))] | add // 0),
          ($u | [.[] | (.output_tokens // 0)] | add // 0)' 2>/dev/null)
      if [ -n "${ADD[0]:-}" ] && [ -n "${ADD[1]:-}" ] && [ -n "${ADD[2]:-}" ]; then
        case "${ADD[0]}${ADD[1]}${ADD[2]}" in
          ''|*[!0-9]*) ;;                       # partial line mid-write: retry next run
          *) fresh=$((fresh + ADD[0]))
             cached=$((cached + ADD[1]))
             outtok=$((outtok + ADD[2]))
             off=$size ;;
        esac
      fi
      printf '%s %s %s %s\n' "$off" "$fresh" "$cached" "$outtok" > "$state" 2>/dev/null
    fi
  fi
fi

case "$permjson" in
  acceptEdits)       perm='Accept edits' ;;
  plan)              perm='Plan mode' ;;
  bypassPermissions) perm='Bypass permissions' ;;
  auto)              perm='Auto' ;;
  *)                 perm='Ask for approval' ;;
esac

# ---- session tokens, humanized ---------------------------------------------
# "in" is fresh + cached; the cached segment is that share of the total input.
read -r total_str ratio_str out_str < <(awk -v f="$fresh" -v c="$cached" -v o="$outtok" 'BEGIN {
  t = f + c
  printf "%s %.0f%% %s\n", fmt(t), (t > 0 ? c * 100 / t : 0), fmt(o)
}
function fmt(n) {
  if (n >= 1000000)    return sprintf("%.1fM", n / 1000000)
  else if (n >= 100000) return sprintf("%.0fk", n / 1000)
  else if (n >= 1000)  return sprintf("%.1fk", n / 1000)
  else                 return sprintf("%d", n)
}')

# ---- palette (Catppuccin-ish, bright enough for the dimmed status row) -----
rst=$'\033[0m'
dim=$'\033[2m'
C_SEP=$'\033[38;2;108;112;134m'    # dim grey separator
C_MODEL=$'\033[38;2;250;179;135m'  # peach
C_DIR=$'\033[38;2;166;227;161m'    # green
C_BRANCH=$'\033[38;2;137;180;250m' # blue
C_ADD=$'\033[38;2;166;227;161m'
C_DEL=$'\033[38;2;243;139;168m'
C_OK=$'\033[38;2;166;227;161m'
C_BUSY=$'\033[38;2;249;226;175m'
C_PERM=$'\033[38;2;250;179;135m'
C_IN=$'\033[38;2;137;220;235m'     # sky
C_CACHED=$'\033[38;2;180;190;254m' # lavender
C_OUT=$'\033[38;2;250;179;135m'    # peach
C_FAST=$'\033[38;2;250;179;135m'

# ---- segments --------------------------------------------------------------
parts=()
if [ -n "$effort" ]; then
  parts+=("${C_MODEL}${model}${dim} ${effort}${rst}")
else
  parts+=("${C_MODEL}${model}${rst}")
fi
[ -n "$dname" ]  && parts+=("${C_DIR}${dname}${rst}")
[ -n "$branch" ] && parts+=("${C_BRANCH}${branch}${rst}")
parts+=("${C_ADD}+${added}${rst} ${C_DEL}-${removed}${rst}")
if [ "$status" = 'Working' ]; then
  parts+=("${C_BUSY}Working${rst}")
else
  parts+=("${C_OK}Ready${rst}")
fi
parts+=("${C_PERM}${perm}${rst}")
parts+=("${C_IN}${total_str} in${rst}")
parts+=("${C_CACHED}${ratio_str} cached${rst}")
parts+=("${C_OUT}${out_str} out${rst}")
parts+=("${C_FAST}Fast ${fast}${rst}")

out="${parts[0]}"
for p in "${parts[@]:1}"; do out="${out} ${C_SEP}·${rst} ${p}"; done
printf '%s\n' "$out"
