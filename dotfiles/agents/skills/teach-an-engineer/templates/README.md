# HTML templates

Pick **one** of these templates and reuse it. Do **not** hand-build a new template
unless the user explicitly asks for a custom look. Each file is a complete,
self-contained skeleton (layout + CSS + KaTeX) — copy it, recolor it, and fill in
the placeholder sections.

## Quick pick

| Template file | Style | Look & feel | Best when | Math |
|---|---|---|---|---|
| `warm-serif.html` | Warm serif | Warm paper `#fdfcf8`, serif (Source Serif / Georgia), gold `#d4a72c` title underline + topbar, soft callout blocks | Math-heavy, long-form explanation with a lecture-note rhythm | KaTeX |
| `navy-hero.html` | Navy hero | Light-gray page `#f3f5f9`, sans (system-ui), sticky pill bottom-nav, navy-gradient hero card with chips, white cards + card grid | Broad overview, card-based sections, many side-by-side ideas | KaTeX |

> Both are **light mode** (light background, dark text). The "navy hero" file's
> hero is a dark navy gradient **card**, but the page and body stay light.

## Template details

### `warm-serif.html` — warm serif
- **Font:** Source Serif Pro / Georgia / Times, serif; base 17px.
- **Palette:** bg `#fdfcf8`, ink `#1a1a1a`, heading underline gold `#d4a72c`,
  link `#9a5b00`; equation block bg `#f7f4ec`; intuition blue `#4a7bb5` on
  `#eef4fb`; takeaway green `#3a7d44` on `#edf5ec`.
- **Structure:** sticky bottom jumplist `.toc` nav (section anchors), `<h1>`
  title + one-paragraph hero, numbered `<h2>` sections, centred `.eq` blocks,
  `.callout` (`intuition` / `takeaway`), `.figure`, `table`, `<hr>` dividers.

### `navy-hero.html` — navy hero
- **Font:** system-ui / Helvetica / Arial, sans; base 16.5px.
- **Palette (CSS vars):** `--bg:#f3f5f9`, `--card:#ffffff`, `--ink:#1f2a44`,
  `--muted:#5a6a84`, `--soft:#e7ebf3`, `--accent:#2b5ad8`,
  `--accent-soft:#e8edfc`, `--teal:#0e8f83`, `--amber:#b45309`,
  `--line:#dbe2ee`.
- **Structure:** sticky pill `nav`, gradient hero header (kicker + title + lead +
  chips), sections with numbered badges, `.card`, `.note`, `.takeaway`,
  `.formula` (tagged), `.fig` + caption, `.zoo` card grid, notation table
  `table.nt`, footer.

## Math & self-contained

Both templates render math with **KaTeX auto-render**, loaded from a CDN along
with `katex.min.css`; inline math uses `\(...\)`, display math uses `\[...\]`.
There is no MathJax CHTML template in this set. To ship a truly offline file,
inline the KaTeX CSS/JS (and web fonts) into the page and drop the CDN
`<link>`/`<script>` tags.

## Reusing a template

1. Copy the chosen template.
2. Update `<title>`, topbar / nav brand, and hero text.
3. Set the accent variables (gold `#d4a72c` values, or `--accent`/`--teal`/…)
   and the body font family if you change the look.
4. Replace the placeholder sections with the topic content, keeping the
   intuition / takeaway callouts from `SKILL.md`.
5. Swap the CDN math for the inlined KaTeX bundle before shipping a
   self-contained file.
