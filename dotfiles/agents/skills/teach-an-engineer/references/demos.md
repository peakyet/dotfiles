# Demos

A demo exists to make **one** idea undeniable — the reader runs it, or reads its few lines, and the mechanism becomes visible. Simplicity is the hard requirement, not a style preference: a demo that itself needs a tutorial has failed. If three lines of worked arithmetic already land the point, skip the demo.

## What is allowed

Language choice follows what the demo must show (verify with `command -v` before committing to one):

| Demo shows | Use |
|---|---|
| Exact math, symbolic checks, a clean reference plot | `wolfram-mcp` skill (see SKILL.md) |
| How *code or an algorithm* behaves — complexity, an accumulating error, a cache/race effect, a state update | **C++** (`g++ -O2`, STL only) or **Python 3** (stdlib first) |
| Numerical simulation — matrices, signals, control loops, quick plots | **Octave** (its numeric + plotting built-ins need no packages) |

Python on a bare system often lacks numpy/matplotlib — prefer stdlib (`math`, `random`, `statistics`, `array`) or write the loop in Octave instead. Same idea, two implementations (naive vs fixed) only when that contrast *is* the lesson.

## Simplicity limits (enforced)

- **One idea, one file, one command.** The first line is a comment with the run/compile command and the one sentence the demo proves.
- **≤ ~60 lines** including comments. Longer → cut scope or drop it.
- Output fits one screen: a handful of printed numbers, a small comparison, or **one** PNG plot.
- No classes, build systems, packages, network, threads, or GUIs — unless the demo is *about* exactly that construct (a tiny thread-race demo for a concurrency topic is fine).
- Comment only the 2–3 lines that carry the mechanism; leave the rest unadorned so its size is obvious.
- Plot to a PNG file and embed the image in the HTML; never require the reader to run anything to get the payoff.

## Verify, then embed

- Actually compile and run the demo; paste its **real** output next to the code in the HTML, and make sure that output matches what the prose claims.
- Wrong or fabricated output is worse than no demo. If no suitable toolchain exists, shrink the demo (print fewer things, use plain arithmetic) or present it as annotated pseudocode — labeled as such — with a hand-computed sample output.
- Save the demo file(s) and any PNGs under `reference/<topic>/` with the HTML (per SKILL.md) so the reader can rerun them.

## Embedded HTML pattern

Wrap the code in a short block that stays scannable:

```html
<details><summary>Demo: why the error doubles each step &nbsp;(simple-chaos.cpp — run: g++ -O2 simple-chaos.cpp && ./a.out)</summary>
<pre><code class="language-cpp">// one-sentence purpose + run command here
...≤60 lines, mechanism lines commented...
$ ./a.out          ← the real output, pasted in
</code></pre></details>
```

Keep the summary line (one sentence + the command) visible even when collapsed, so a skimmer gets the result and the proof command at a glance.

## Pre-delivery check

- [ ] Could a reader recite the single idea the demo proves, unprompted?
- [ ] ≤ ~60 lines, one file, one command, output on one screen?
- [ ] Ran it myself; pasted real output; numbers match the prose?
- [ ] Toolchain choice matches what the demo shows (table above)?
- [ ] Code, output, and plot saved under `reference/<topic>/`?
