# mako notification chimes

macOS/iPhone-flavoured alert tones for mako, referenced from `../config` via
`on-notify=exec paplay $HOME/.config/mako/sounds/<name>.oga`.

| file | used for | character |
| --- | --- | --- |
| `tri-tone.oga` | **every agent notification** | iPhone alert: ascending 1–5–8 marimba arpeggio (D5–A5–D6) |
| `kalimba.oga` | — alternative | the same melody on thumb piano: softer, rounder, wider spacing |

`tri-tone` is the one wired up: it plays for the DSH question *and* for every
completion (DSH finished, Claude Code, codex) — whether the agent reached mako
through a terminal (`app-name=kitty`) or through the browser
(`desktop-entry=zen`, i.e. Amp's web UI). The rest are candidates kept so
that swapping the sound is a one-line edit in `../config` — replace
`tri-tone.oga` in the relevant block with any name above.

## Provenance

These are **synthesised, not sampled**. Apple's alert tones are copyrighted
recordings and are deliberately not redistributed here; what is reproduced is
the character of each sound.

`tri-tone` follows the documented origin of Apple's tone: Kelly Jacklin's
`158-marimba`, written for SoundJam MP in 1998 and inherited by iTunes and then
the iPhone. The digits are scale degrees — root, fifth, octave — not the
root–third–fifth that the "tri-tone" name implies (Jacklin has called the name
musically inaccurate). The marimba timbre comes from a 1 : 4 : 10 partial
stack, which is roughly where a tuned marimba bar rings; `kalimba` reuses the
melody with tine partials near 1 : 6.3 : 17.5.

## Regenerating

Standard library only — no numpy, no sox:

```sh
python3 generate.py              # writes the .oga files
python3 generate.py --keep-wav   # also keep the intermediate 16-bit WAVs
```

ffmpeg transcodes WAV → Ogg Vorbis; without ffmpeg the script leaves WAVs.

Every chime is normalised to a common loudness under a hard ceiling: the
scale is whichever is smaller of "peak to −0.4 dBFS" and "RMS to −16 dBFS",
so a percussive chime is not left far quieter than a sustained one, and
nothing clips.

## Auditioning

```sh
for f in ~/.config/mako/sounds/*.oga; do echo "$(basename "$f")"; paplay "$f"; done
```

To retune, edit the chime functions in `generate.py` and re-run it. Note
frequencies and start times sit at the top of each `render_*` function, so
changing an interval, a spacing or a decay is a one-line edit.
