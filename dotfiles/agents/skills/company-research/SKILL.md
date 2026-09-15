---
name: company-research
description: Company research using Exa. Finds company info, competitors, news, financials, LinkedIn profiles, builds company lists. Use when researching companies, doing competitor analysis, market research, or building company lists.
context: fork
---

# Company Research

## Tool Selection (Critical)

Two Exa surfaces, two jobs:

- **Exa Agent** (`agent_run`) — the default for company research. Use it for deep dives, competitor analysis, multi-angle research (product + funding + news + people), and building company lists. One Agent run handles query decomposition, multi-step searching, and synthesis internally — do not orchestrate many manual searches for work an Agent run covers.
- **`web_search_exa`** — quick, low-latency lookups: a fast company discovery pass, a single news check, or finding a homepage.

Do NOT use `web_fetch_exa` — Agent runs already gather page content, and fetching raw pages yourself just floods context.

## Deep Dives and Lists: Exa Agent

Agent runs may stream to completion in one call. If a run outlives the MCP call window, continue waiting with its returned run ID.

1. Call `agent_run` with a natural-language `query` and, when you want repeatable structure, an `outputSchema` (bound arrays with `maxItems`).
2. If it returns `status: "running"` with a `runId`, call `agent_run` again with only that `runId` until `outputReady` is true.
3. Read `output.text` or `output.structured`, plus `output.grounding` citations, from the `agent_run` result.

Useful inputs: `systemPrompt` (source preferences, dedup rules), `input.exclusion` (companies to avoid), `previousRunId` (a new follow-up run based on a completed run), `effort` (`"low"` default; `"auto"` or `"high"` for more depth).

### Example: company deep dive

```
agent_run {
  "query": "Research Anthropic: product lines, funding history and valuation, key executives, main competitors, and notable news from the last 6 months.",
  "effort": "auto",
  "outputSchema": {
    "type": "object",
    "properties": {
      "overview": { "type": "string" },
      "funding": { "type": "array", "maxItems": 10, "items": { "type": "object", "properties": { "round": { "type": "string" }, "amount": { "type": "string" }, "date": { "type": "string" } }, "required": ["round"] } },
      "competitors": { "type": "array", "maxItems": 10, "items": { "type": "string" } },
      "key_people": { "type": "array", "maxItems": 10, "items": { "type": "object", "properties": { "name": { "type": "string" }, "title": { "type": "string" } }, "required": ["name", "title"] } }
    },
    "required": ["overview", "competitors"]
  }
}
```

### Example: build a company list

```
agent_run {
  "query": "Find 25 AI infrastructure startups headquartered in San Francisco. For each, include what they build and their latest funding stage.",
  "effort": "auto",
  "outputSchema": {
    "type": "object",
    "properties": {
      "companies": {
        "type": "array",
        "maxItems": 25,
        "items": {
          "type": "object",
          "properties": {
            "name": { "type": "string" },
            "website": { "type": "string", "format": "uri" },
            "description": { "type": "string", "description": "in 12 words or less" },
            "funding_stage": { "type": "string" }
          },
          "required": ["name", "website", "description"]
        }
      }
    },
    "required": ["companies"]
  }
}
```

## Quick Lookups: Exa Search

Use `web_search_exa` when a single fast search answers the question. It takes exactly three parameters:

| Parameter | Notes |
|-----------|-------|
| `query` | Required. Describe the *ideal page*, not keywords — `"blog post comparing React and Vue performance"`, not `"React vs Vue"`. |
| `numResults` | Default 10. Raise for a discovery sweep, lower for a targeted check. |
| `objective` | Goal for this search turn: which documents should rank first, which to exclude, what facts or figures to pull. This is the strongest lever — use it. |

### Focusing by category

There is no separate category field — embed it in the query string:

- `category:company` → homepages with rich metadata (headcount, location, funding, revenue)
- `category:people` → public professional profiles
- no category → general web results, broader context

### Express filters in natural language

This surface has **no** domain, date, or exact-text filter parameters. Constraints go into `query` or `objective`:

- instead of a date filter → `"2026 coverage of ..."` in the query
- instead of `excludeDomains` → "exclude vendor marketing pages" in `objective`
- instead of exact-text → quote the phrase in the query

### Examples

Discovery pass:
```
web_search_exa {
  "query": "category:company AI infrastructure startups San Francisco",
  "numResults": 20,
  "objective": "Rank company homepages first; prefer those listing headcount and funding stage."
}
```

News check:
```
web_search_exa {
  "query": "Anthropic AI safety announcements",
  "numResults": 15,
  "objective": "Rank 2026 press coverage first; exclude opinion pieces and aggregators."
}
```

Key people:
```
web_search_exa {
  "query": "category:people VP Engineering AI infrastructure",
  "numResults": 20,
  "objective": "Rank current VP-level engineering leaders at infrastructure companies first."
}
```

## Token Isolation

Never dump raw search results into main context. Spawn Task agents for `web_search_exa` calls; for Agent runs, go straight from `output.structured` to the final answer.

## Browser Fallback

Fall back to Claude in Chrome only when content is auth-gated or requires JavaScript rendering.

## Output Format

Return:
1) Results (structured list; one company per row)
2) Sources (URLs; 1-line relevance each — use `output.grounding` from Agent runs)
3) Notes (uncertainty/conflicts)

## References

- Exa Agent guide: https://docs.exa.ai/reference/agent-api-guide
- Company Search reference: https://docs.exa.ai/reference/verticals/company-for-coding-agents
- Exa MCP setup: https://docs.exa.ai/reference/exa-mcp
- Full docs for LLMs: https://docs.exa.ai/llms.txt
