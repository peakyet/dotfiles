---
name: teach-an-engineer
description: Explain a topic like I'm a well-educated engineer. Use when the user types /teach-an-engineer with a topic, or asks for a picture explainer of how something works.
---

# teach-an-engineer

Create a single HTML explanation for an engineer who is new to the topic but comfortable with algebra and basic calculus. Build intuition first, then add rigor; explain the underlying why and how rather than presenting formulas or definitions to memorize. Research the topic before writing.

Governing rule: prose is the scarce resource. Explain the **motivation** and the **core idea** carefully and thoroughly — everywhere else demonstrate with figures, examples, and demos in a few brief sentences, and keep the language plain.

## Requirements

- First read `templates/README.md` and use the `warm-serif.html` template there, with a light background and dark text throughout. Do not create a custom template unless the user requests one.
- Save the HTML and, after completion, the final markdown summary under `reference/<topic>/`. Keep any code, scripts, notebooks, or demos written for the explanation there as well; do not delete them after the task, since the user may learn from them.
- Spend depth where it counts: the motivation and the core idea. Before introducing an idea, state the problem it is meant to solve and why solving it matters, then work through the idea itself patiently — plain words, concrete examples, one step at a time — until the reader gets it. This is the only prose allowed to run long.
- Keep prose brief everywhere else: a few sentences per claim, no filler, no paragraph that restates what a figure or demo already shows, and no text the reader can skip without losing the thread. Explain why each claim or step matters, and end every section with a one-sentence takeaway.
- Minimize terminology: prefer plain language over jargon, introduce a term only when the reader genuinely needs the name, and anchor it to an example or figure instead of a dictionary definition. Never define a term with other terms, and do not name-drop theory to impress.
- Keep the reader reading: follow [references/engagement.md](references/engagement.md). Open the first screen with a question, paradox, or concrete failure — never a definition — and use curiosity gaps, prediction prompts, productive-failure moments, open/close loops, and a single running example. Every hook must pay off in the mechanism; cut interesting-but-irrelevant decoration (seductive details hurt learning).
- Demonstrate, don't describe: a key idea is carried by a figure, a worked example, or a small demo — not a wall of text; a key idea explained only in prose is unfinished. Prefer a figure for each key idea (follow [references/figures.md](references/figures.md)), and include a concrete worked example plus a small demo when useful.
- Demos are not Wolfram-only: plain C++, Python, or Octave are allowed (and often clearer for behavioral code demos). They must stay minimal — one idea, one file, ≤ ~60 lines, output that fits one screen, real verified output only. Follow [references/demos.md](references/demos.md).
- Define symbols on first use and keep notation consistent. Render equations with the chosen template's math engine at LaTeX quality, including proper variables, spacing, fractions, exponents, and operators.
- Progress from the core idea to variations and improvements, then connect it to related theory and its broader framework.
- For mathematical or scientific computation, verification, or reference plots, load and use the `wolfram-mcp` skill instead of writing verification code. Prefer exact results and embed labelled plots/visualizations with legends. Use plain-language demos (per above) only to show behavior, not to verify math.

After the artifact:
- Deliver the finished HTML, then pause and invite the user to ask questions about the topic.
- Wait for their questions; answer each one briefly and plainly — with a figure, example, or demo where possible, never a wall of text or jargon.
- Revise the HTML in place to incorporate surfaced details, corrections, or clarifications while preserving its visual and LaTeX style, then show the revised artifact again. Put each change in the conceptually correct section or subsection, not at the end; add a new section only for a genuinely new concept.
- When questions are finished, test understanding section by section with deeper, insight-focused questions. Correct misconceptions and update the HTML until each section is understood.
- After the comprehension check, write a standalone markdown summary in the topic folder containing only the final understanding, not the Q&A.
- End with a concise list of reputable recommended resources for deeper research, such as books, papers, documentation, or courses, with a brief note explaining what each adds.

Topic: $ARGUMENTS
