---
name: teach-an-engineer
description: Explain a topic like I'm a well-educated engineer. Use when the user types /teach-an-engineer with a topic, or asks for a picture explainer of how something works.
---

# teach-an-engineer

Produce a single HTML explanation that teaches the topic to a well-educated engineer who is new to it but comfortable with basic math. Build intuition first, then support it with rigor: use equations where they clarify, but never let algebra replace a mental picture.

Target reader: comfortable with algebra and basic calculus; wants the "why" and the underlying mechanism, not a formula to memorize.

Ground the explanation in accurate, gathered information about the topic. Research it before writing.

Calculations:
- When you need a mathematical or scientific calculation — to check a value, evaluate a symbol, solve, integrate, differentiate, or plot — use the `wolfram-mcp` skill (the Wolfram Language MCP server) instead of writing your own verification code. Load and follow that skill's instructions before computing, and prefer exact results over approximations, with labelled plots.
- Use the Wolfram tools to create plots and visualizations that illustrate the topic, and embed them in the artifact with clear labels and legends.

Guide the explanation with these principles:
- Pick a template first. Open `templates/README.md` for a one-page comparison of
  the available styles and choose ONE of them — never hand-build a new template
  unless the user explicitly asks for a custom look. If the user gives no
  preference, offer the styles and ask which they want; if they don't care,
  default to the best fit (e.g. `warm-serif.html` for a math-heavy,
  long-form note, `navy-hero.html` for a broad, card-based overview).
- Save every generated file — the HTML explanation and the markdown document — into a folder named after the topic inside `reference/` (e.g. `reference/gradient-descent/` for "gradient descent"), keeping that topic's files together.
- Use light mode: a light background with dark text throughout the artifact, no dark theme. The templates in `templates/` already follow this — pick one of them.
- Spend the depth on the **why** and the **how**: the mechanism, the reasoning, the
  intuition, and each step of the derivation. Keep the **what** (definitions,
  terminology, taxonomy, naming) to a brief introduction and then move on — a
  definition the reader can look up elsewhere does not deserve its own section. The
  value of this artifact is the why and the how, not a re-description of what the
  thing is.
- Prefer figures over prose — use a figure to explain and teach each key idea.
- When building figures, follow the layout and inspection rules in [references/figures.md](references/figures.md).
- Lead with a short motivation: the problem it solves and why it matters.
- Before every claim or derivation step, state its motivation first: explain why it is worth doing or including, and never assert a fact without its "why".
- Use a concrete worked example and, where useful, a small demo to show how the theorem applies.
- Derive concisely; justify every step in a clause and leave no unexplained jumps.
- End each section with a one-sentence takeaway, and connect steps so no gaps hide.
- Define every symbol on first use and keep notation consistent.
- Typeset equations to LaTeX quality: italic variables, proper spacing, and correct structures for fractions, exponents, and operators. Render them with the math engine the chosen template uses (KaTeX auto-render) so they read like LaTeX. Loading KaTeX from a CDN is fine — there is no need to inline the bundle or avoid pre-rendered SVG math unless the user asks.
- Move from the core idea to its variations and improvements.
- Tie the idea to related theory and show where it fits a larger framework.

After the artifact:
- Deliver the finished HTML, then pause and invite the user to ask questions about the topic.
- Wait for their questions; answer each one clearly.
- Update the HTML to incorporate the detail those questions surface, keeping the same visual and LaTeX style, then show the revised artifact again.
- After the user finishes their questions, check their understanding section by section by asking deeper questions that probe insight, not just recitation; stop once each section is correct. If they get something wrong, correct the misconception and update the HTML accordingly.
- Once the user passes the comprehension check, write a standalone markdown document in that topic folder (e.g. `reference/gradient-descent/gradient-descent.md`) that summarizes only the final understanding of the topic — not the Q&A detail.

Topic: $ARGUMENTS
