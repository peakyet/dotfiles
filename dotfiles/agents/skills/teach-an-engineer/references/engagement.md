# Keeping the reader reading

Clarity alone does not hold a reader: they re-decide every few sections whether to keep scrolling. Use the research-backed moves below to make them *want* to. One rule governs all of them — **every hook must pay off in the mechanism being taught.** Interesting-but-irrelevant decoration measurably *hurts* comprehension (see "seductive details" at the end).

## The first screen

Open with a question, a paradox, a concrete failure, or a surprising number — never a definition. State the stakes: what breaks, what stays impossible, or what behaves perversely until this idea exists. The skill already requires stating the problem the idea solves; go one step further and phrase it as a question the reader now wants answered. (Novelty triggers interest; usefulness and felt progress are what maintain it — see #6.)

## Techniques

### 1. Open a curiosity gap
- **Evidence.** Curiosity is "cognitively induced deprivation that arises from the perception of a gap in knowledge or understanding" (Loewenstein 1994, Psychological Bulletin 116(1):75–98, <https://doi.org/10.1037/0033-2909.116.1.75>). While a gap is open, the brain's reward–hippocampus circuitry is engaged and memory improves even for *incidental* material encountered in that state (Gruber, Gelman & Ranganath 2014, Neuron 84(2):486–496, <https://doi.org/10.1016/j.neuron.2014.08.060>). Popularized as the "gap theory of curiosity" in Heath & Heath, *Made to Stick* (2007).
- **Apply.** Name the question a section answers before answering it — lead-in lines like "Why does this loop go unstable at 3 a.m. and not at noon?", and consider question-form section headings. One gap per section, opened early, filled by the takeaway.

### 2. Break, then repair, an expectation
- **Evidence.** Surprise — a violated prior schema — is what makes people ask questions (Loewenstein 1994; *Made to Stick* ch. 3).
- **Apply.** Surface the reader's naive model ("you'd expect the error to shrink as you add capacity…"), break it in one sentence ("it actually doubles"), then repair with mechanism. Two or three well-placed surprises per article beat constant shock.

### 3. Let them fail at the problem first
- **Evidence.** Productive failure: when a solution arrives "well-assembled," learners never see *why* its pieces fit together; grappling with the problem beforehand — even failing at it — makes the canonical explanation stick (Kapur 2008, Cognition and Instruction 26(3); overview at <https://www.manukapur.com/productive-failure/>).
- **Apply.** Before a derivation, pose the mini-problem it solves and let the obvious attempt visibly break — one sentence or one line of a demo — *then* introduce the machinery as the rescue. Frame formulas as answers to a fight the reader just had.

### 4. Ask before revealing
- **Evidence.** Prequestioning/pretesting: guessing *before* learning improves later memory and transfer, provided the correct answer is studied immediately afterwards (open-access review, Educational Psychology Review 35:97, 2023, <https://link.springer.com/article/10.1007/s10648-023-09814-5>).
- **Apply.** Include 2–4 "predict first" prompts: "What happens to the steady-state error when you double \(k\) — commit to a guess, then check." Reveal the answer and *why the wrong guess feels right* immediately after. Say explicitly that wrong guesses are valuable — they are, per #3.

### 5. Open loops early, close every one
- **Evidence.** A 2025 meta-analysis found the classic Zeigarnik memory advantage does not replicate, but people *robustly resume interrupted/unfinished tasks* (the Ovsiankina effect; Humanities and Social Sciences Communications, <https://www.nature.com/articles/s41599-025-05000-w>). Unfinished narrative pulls readers onward; unresolved promises just burn trust.
- **Apply.** Tease across sections: "this quirk is why the whole scheme works — §4 collects the debt." Close every loop the article opens (or delete the tease). Never leave the reader's central question hanging at the end.

### 6. Maintain with meaning, not just novelty
- **Evidence.** Interest develops in phases — triggered situational interest (spark) only survives if maintained situational interest (purpose, personal meaning, comprehensibility) takes over (Hidi & Renninger 2006, Educational Psychologist 41(2):111–127, <https://eric.ed.gov/?id=EJ736298>).
- **Apply.** Hooks trigger; *relevance maintains*. Tie each section back to something the reader cares about — their systems, a failure mode, the running example — and make sure each section leaves them understanding more, not just more intrigued.

### 7. Write to a person
- **Evidence.** Personalization principle: people learn better from multimedia in conversational "you/we" style than in formal prose (Mayer; principles summarized with citations at <https://sites.uw.edu/somlearningtech/principles-for-teaching-with-multimedia/>).
- **Apply.** "You hand the controller a step input…", "let's sanity-check that." Contractions welcome. Formal/passive voice costs engagement for zero information gain.

### 8. Protect the reading flow
- **Evidence.** Signaling principle: highlighting where the reader is and what matters improves learning; coherence (cut extraneous material) and segmentation (learner-paced chunks) reduce drop-off from confusion (Mayer, same link).
- **Apply.** The one-sentence section takeaway is your signal; add a one-line bridge ("next: why this breaks under delay"). Keep paragraphs short, define a symbol before it becomes load-bearing. Confused readers quit silently — fog, not boredom, kills most explainers.

### 9. Grow one running example
- **Apply.** Pick one concrete system early (one motor loop, one dataset, one cache) and extend it through the whole article so each new abstraction is tested on a familiar instance. The example carries narrative thread through the math, and it keeps the figures consistent (see [figures.md](figures.md)). Make the reader attached to *this* system's fate.

## Seductive details — the anti-pattern

- **Evidence.** Passages decorated with interesting-but-irrelevant material produced *worse* recall of main ideas and *less* transfer than undecorated ones; the decorations prime inappropriate schemas (Harp & Mayer 1998, Journal of Educational Psychology 90(3):414–434, <https://eric.ed.gov/?id=EJ576496>).
- **Apply.** Every anecdote, trivia digression, and joke must point at the mechanism. If a hook never pays off later in the explanation, delete it — it is actively stealing comprehension.

## HTML recipes (drop into any template)

- **Predict-then-reveal:** `<details><summary>Predict: what happens to the overshoot when k doubles?</summary> Answer, and why the tempting guess fails. </details>` — style like the intuition callout. Free interactivity, no JS.
- **Loop tease/payoff:** a short italic tease line under a section opener; the section's takeaway box closes it.
- **Skimmer's spine:** the hero question + the sequence of section takeaways should read as a coherent mini-article on their own. Test by reading only those; fix if it doesn't.

## Pre-delivery checklist

- [ ] First screen opens a question/puzzle the rest of the page answers — no definition-first
- [ ] 2–4 prediction prompts, each answered immediately after
- [ ] ≥1 moment where the reader's naive approach visibly fails before the real machinery arrives
- [ ] ≥1 surprise that gets *explained* by the mechanism
- [ ] Every loop teased in the intro is collected by the end
- [ ] Conversational voice; "you/we" present
- [ ] One running example threaded through sections
- [ ] Nothing interesting that doesn't serve the mechanism
