---
name: tavily-dynamic-search
description: |
  Web research with context isolation. Use this skill when a research task requires searching the web, triaging results, and extracting specific information across multiple sources — without flooding the caller's context window with raw pages and boilerplate. Triggers on "research", "search and filter", "find the important parts", "compare X vs Y", "what does the literature say", or any multi-source investigation needing a curated, noise-free answer. For a single quick fact or one known URL, call the Tavily MCP tools directly instead.
context: fork
---

# Tavily Dynamic Search

Search the web, filter results, and extract content so that **raw pages never reach the caller's context window**. Only your final curated digest comes back.

## Why this matters

`tavily_search` with `include_raw_content` can return 15 results × 30-50K chars each — **~500K characters** of navigation bars, cookie banners, and boilerplate. If that lands in a conversation context, it burns tokens and degrades reasoning quality under the noise.

This skill runs in a **forked subagent**. The fork boundary is the sandbox: every `tavily_*` tool result stays in *your* context, and only your final report crosses back to the caller. A caller that would have absorbed 500K characters receives ~2-4K of pure signal.

## How isolation is achieved

MCP tools are model-invoked, not script-invoked — you cannot call them from inside a `python3` heredoc the way you would a CLI. The isolation therefore comes from the fork, not from a subprocess:

- **Inside the fork** — call `tavily_search` / `tavily_extract` freely. Intermediate results are yours alone. Triage, discard, re-query as much as the task needs.
- **Leaving the fork** — you return one final message. That is the *only* thing the caller sees.

So the discipline is: be verbose in your tool use, ruthless in your report. Never paste raw page text into your final answer.

## Core rule

**Never let raw page content into your final report.** Filter first, quote selectively, cite URLs.

Before including any quote, ask: does the caller need this exact text, or do they need the fact it establishes? Usually the latter — with the URL as the receipt.

## Tool Selection

Two tools cover almost everything:

| Tool | Use for |
|------|---------|
| `tavily_search` | Discovery — titles, URLs, snippets, scores. `.results[].content` is a ~500-1500 char snippet. |
| `tavily_extract` | Depth — full markdown for URLs you have already chosen. |

Three more for structure and synthesis:

| Tool | Use for |
|------|---------|
| `tavily_map` | List URLs on a domain without extracting. Use when you know the site but not the page. |
| `tavily_crawl` | Bulk-extract many pages under a path (`select_paths`). Use for "all of the docs section". |
| `tavily_research` | Hand off an entire multi-source research question. Returns a synthesized report. Takes 30-120s. |

### Mind the configured defaults

This MCP server is configured with `DEFAULT_PARAMETERS` = `{"include_images": true, "max_results": 15, "search_depth": "advanced"}`. So searches already run at **advanced depth with 15 results and images on**. For lean research, override `include_images: false` (image URLs are pure noise for text questions) and set `max_results` explicitly — 15 results of raw content is a lot to triage.

## Key parameters

### `tavily_search`

| Parameter | Notes |
|-----------|-------|
| `query` | Required. |
| `max_results` | Default 5 (server default here: 15). |
| `search_depth` | `basic` (default), `advanced`, `fast`, `ultra-fast`. Server default: `advanced`. |
| `include_raw_content` | Full page text inline. **Use sparingly** — this is what floods context. Prefer `tavily_extract` on chosen URLs. |
| `include_domains` / `exclude_domains` | Arrays. Whitelisting is often the single highest-leverage filter. |
| `time_range` | `day` / `week` / `month` / `year` — for "what's the latest". |
| `country` | Boost results from a country (full name, e.g. `"Germany"`). Only when `topic` is general. |
| `start_date` / `end_date` | `YYYY-MM-DD` bounds. |
| `exact_match` | Restrict to results containing your quoted phrase. |

### `tavily_extract`

| Parameter | Notes |
|-----------|-------|
| `urls` | Required, array. |
| `extract_depth` | `basic` (default) or `advanced` — use `advanced` for LinkedIn, protected sites, tables. |
| `format` | `markdown` (default) or `text`. |
| `query` | Reranks returned chunks toward a query — **very useful**: it does relevance filtering server-side, so you receive less to filter yourself. |

Results come back split into `results` (succeeded) and `failed_results`. **Check `failed_results` and drop them silently** — do not report a source you could not read.

## The workflow: triage, then extract

Do not search and extract in one breath. Triage first — you cannot filter well before you know what you have.

**Step 1 — discover.** Run one or more `tavily_search` calls aimed at different angles. Keep `include_raw_content` off. You get titles, URLs, scores, snippets.

**Step 2 — triage.** Read the snippets and rank. Which 2-4 sources actually bear on the question? Which are SEO filler, forums, or stale? Notice agreement and conflict across sources — that is signal about the answer, not just about the sources.

**Step 3 — extract.** Call `tavily_extract` on your chosen URLs only, with a `query` tuned to what you need. Use `extract_depth: advanced` when the content is table-heavy or protected.

**Step 4 — filter and synthesize.** Pull the specific facts, figures, dates, and quotes you need. Then write the digest.

For a question you cannot answer from step 2 snippets, iterate: new query → new triage → new extract. Follow leads (a document named in one source, an author, a dataset) rather than re-running variations of your first query.

## Writing the digest

Structure your final report as:

1. **Answer** — the finding, up front. If the sources disagree, say so and give the range.
2. **Evidence** — the specific supporting facts and figures, each with its URL and a one-line relevance note.
3. **Confidence and gaps** — what was thin, contradicted, or unfindable. Never paper over this.

Target **150-600 tokens per source**, and prefer fewer, better sources. If you are about to return 5,000+ chars from one page, filter harder — but a critical data table is worth keeping intact.

**Attribute everything.** A claim without a URL is not a finding. Where two sources conflict, present both rather than silently picking one.

**Report failure honestly.** If searches returned nothing useful, say that. A confident-sounding synthesis over thin sources is the worst possible output.

## Anti-patterns

- **Dumping raw extracts** into the final report "so the caller can judge". That defeats the entire skill — filter it yourself.
- **Extracting every result.** Triage exists for a reason; `tavily_extract` on 15 URLs at advanced depth is expensive and mostly wasted.
- **One query, one pass.** Real questions usually need a discovery round before you know the right keywords.
- **Reporting a source you couldn't read.** Drop `failed_results`; do not cite what you did not see.
- **Ignoring dates.** On anything time-sensitive, check publication dates and prefer `time_range` — stale results are a common failure mode.

## Fallback

If `context: fork` is unavailable in this harness, spawn a Task/Agent subagent and give it this skill's instructions — the isolation property is preserved as long as the raw tool results stay in the subagent and only a digest returns.

## References

- Tavily search API: https://docs.tavily.com/documentation/api-reference/search
- Tavily extract API: https://docs.tavily.com/documentation/api-reference/extract
- For SDK integration work (writing Tavily into your own code), see the `tavily-best-practices` skill.
