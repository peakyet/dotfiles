---
name: teach-an-engineer
description: Explain a topic like I'm a well-educated engineer. Use when the user types /teach-an-engineer with a topic, or asks for a picture explainer of how something works.
---

# teach-an-engineer

Create a single HTML explanation for an engineer who is new to the topic but comfortable with algebra and basic calculus. Build intuition first, then add rigor; explain the underlying why and how rather than presenting formulas or definitions to memorize. Research the topic before writing.

## Requirements

- First read `templates/README.md`, choose exactly one existing template, and use a light background with dark text throughout. Ask the user to choose when they express no preference; otherwise choose the best fit. Do not create a custom template unless requested.
- Save the HTML and, after completion, the final markdown summary under `reference/<topic>/`.
- Emphasize mechanism, intuition, reasoning, and concise derivations. Keep definitions and terminology brief. Begin with motivation, explain why each claim or step matters, and end every section with a one-sentence takeaway.
- Prefer a figure for each key idea. Follow [references/figures.md](references/figures.md), and include a concrete worked example plus a small demo when useful.
- Define symbols on first use and keep notation consistent. Render equations with the chosen template's math engine at LaTeX quality, including proper variables, spacing, fractions, exponents, and operators.
- Progress from the core idea to variations and improvements, then connect it to related theory and its broader framework.
- For mathematical or scientific computation, verification, or plotting, load and use the `wolfram-mcp` skill instead of writing verification code. Prefer exact results and embed labelled plots/visualizations with legends.

After the artifact:
- Deliver the finished HTML, then pause and invite the user to ask questions about the topic.
- Wait for their questions; answer each one clearly.
- Revise the HTML in place to incorporate surfaced details, corrections, or clarifications while preserving its visual and LaTeX style, then show the revised artifact again. Put each change in the conceptually correct section or subsection, not at the end; add a new section only for a genuinely new concept.
- When questions are finished, test understanding section by section with deeper, insight-focused questions. Correct misconceptions and update the HTML until each section is understood.
- After the comprehension check, write a standalone markdown summary in the topic folder containing only the final understanding, not the Q&A.

Topic: $ARGUMENTS
