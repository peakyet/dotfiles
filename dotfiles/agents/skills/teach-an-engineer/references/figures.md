# Figures

Prefer figures over prose — use them to explain and teach each key idea.

Build and verify the figures with these rules:
- Some big, self-explanatory figures per idea; each figure should carry a single concept.
- Build each figure as HTML <div>s laid out with CSS grid/flex, so MathJax renders equations natively inside them; overlay a small inline <svg> only for geometric primitives like axes, circles, and arrows — never put text or math inside an SVG.
- Before finishing, inspect every figure: verify elements sit in the right place, do not overlap or clip, and any visible equations or labels are correct — fix problems rather than leaving them.

## Wolfram figures

Generate plots, curves, and other visualizations with the Wolfram tools (`wolfram-mcp`) and embed them in the artifact as image assets — they carry their own labels and legends. Keep the authored overlay SVG geometry-only (axes, circles, arrows) and put all equations in HTML via MathJax, so a Wolfram plot is used for data and curves rather than for text or math placement.

## Recommended patterns (optional)

These improve layout and legibility; treat them as suggestions, not additional requirements.

- **Single coordinate system.** Pick a figure size (W×H), draw the SVG with `viewBox="0 0 W H"`, and position the text and math divs from the same coordinates (`left = x/W*100%`, `top = y/H*100%`). Give the container `aspect-ratio` and `max-width` so the whole figure scales as one unit and labels can't drift off the axes.
- **Keep math in the HTML layer.** Use MathJax's CHTML output (renders math as HTML/CSS spans) so no math sits inside the SVG. For a truly self-contained file, embed the CHTML web fonts (e.g. WOFF2 as data URIs); this adds file size, so decide up front.
- **Safe equation blocks.** Put each display equation in its own centered block with vertical padding and `overflow: visible`, so tall fractions, integrals, and sub/superscripts don't clip. Keep `line-height` uniform so inline math aligns to the surrounding text baseline.
- **Minimal styling.** One base font size and spacing unit, one accent color on the light background, subtle fills/borders with rounded corners to group parts, and arrows drawn in the SVG layer (e.g. `marker-end`) rather than over the text.
