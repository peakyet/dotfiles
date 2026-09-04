---
name: teach-an-engineer
description: Explain a topic like I'm a well-educated engineer. Use when the user types /teach-an-engineer with a topic, or asks for a picture explainer of how something works.
---

# teach-an-engineer

Produce a single, self-contained HTML explanation (one file, all assets embedded, no network dependencies) that teaches the topic to a well-educated engineer who is new to it but comfortable with basic math. Build intuition first, then support it with rigor: use equations where they clarify, but never let algebra replace a mental picture.

Target reader: comfortable with algebra and basic calculus; wants the "why" and the underlying mechanism, not a formula to memorize.

Ground the explanation in accurate, gathered information about the topic. Research it before writing.

Restrictions:
- Do not write code to verify the theorem. Verification code is forbidden.
- The only exception is code used purely for presentation or a demo (e.g. an interactive widget or simulation that illustrates the concept). Such code must not check, prove, or assert the result.

Guide the explanation with these principles:
- Save every generated file — the HTML explanation and the markdown document — into a folder named after the topic inside `reference/` (e.g. `reference/gradient-descent/` for "gradient descent"), keeping that topic's files together.
- Use light mode: a light background with dark text throughout the artifact, no dark theme.
- Prefer figures over prose — use a figure to explain and teach each key idea.
- When building figures, follow the layout and inspection rules in [references/figures.md](references/figures.md).
- Lead with a short motivation: the problem it solves and why it matters.
- Use a concrete worked example and, where useful, a small demo to show how the theorem applies.
- Derive concisely; justify every step in a clause and leave no unexplained jumps.
- End each section with a one-sentence takeaway, and connect steps so no gaps hide.
- Define every symbol on first use and keep notation consistent.
- Typeset equations to LaTeX quality: italic variables, proper spacing, and correct structures for fractions, exponents, and operators. Render them with MathJax embedded in the page so they read like LaTeX and stay self-contained — no CDN and no pre-rendered SVG math.
- Move from the core idea to its variations and improvements.
- Tie the idea to related theory and show where it fits a larger framework.

After the artifact:
- Deliver the finished HTML, then pause and invite the user to ask questions about the topic.
- Wait for their questions; answer each one clearly.
- Update the HTML to incorporate the detail those questions surface, keeping the same visual and LaTeX style, then show the revised artifact again.
- After the user finishes their questions, check their understanding section by section by asking deeper questions that probe insight, not just recitation; stop once each section is correct. If they get something wrong, correct the misconception and update the HTML accordingly.
- Once the user passes the comprehension check, write a standalone markdown document in that topic folder (e.g. `reference/gradient-descent/gradient-descent.md`) that summarizes only the final understanding of the topic — not the Q&A detail.

Topic: $ARGUMENTS
