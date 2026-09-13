# Figures

Figures are a primary teaching medium. Give each central idea one primary figure when a visual representation genuinely carries the mechanism; do not add several decorative variants just to satisfy a quota. Text around a figure should motivate it before and state its takeaway after, while the figure carries the mechanism.

Build and verify figures with these rules:

- Choose the representation that exposes the idea: HTML/CSS for layouts, states, and comparisons; inline SVG for geometry and annotated diagrams; a generated plot for measured data, dense curves, or computed results. Do not force a plot into hand-authored SVG when the plot itself is the evidence.
- Keep explanatory math in the HTML layer so KaTeX can typeset it. Use SVG text only for short labels that are part of the geometry; use a generated image or SVG for a plot only when its labels and legend can remain legible.
- Give every figure one clear visual claim, a semantic `<figure>`/`<figcaption>`, and a useful alt text or adjacent text equivalent. Put the main takeaway in the caption or immediately after it, not inside an unreadable graphic.
- Make responsive figures from a stable coordinate system. Use an explicit `viewBox` and `aspect-ratio` for SVG/overlay layouts, keep labels inside the bounds, and avoid absolute-positioned text whose position drifts away from the geometry.
- Before finishing, inspect every figure at desktop and narrow widths when possible: verify that elements sit where they belong, nothing overlaps or clips, and every visible equation, axis, label, and legend is correct. Fix problems rather than leaving them.
