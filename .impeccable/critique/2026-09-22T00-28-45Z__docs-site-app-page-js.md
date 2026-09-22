---
target: docs-site docs page
total_score: 30
max_score: 40
na_heuristics: 
p0_count: 0
p1_count: 2
timestamp: 2026-09-22T00-28-45Z
slug: docs-site-app-page-js
---
# LangBIOS docs-site critique

Method: dual-agent (A: isolated subagent, delivered full report before session rate-limit · B: isolated subagent, completed cleanly)

## Design Health Score

| # | Heuristic | Score | Key Issue |
|---|-----------|-------|-----------|
| 1 | Visibility of System Status | 2 | No active-section indicator despite persistent sidebar |
| 2 | Match System / Real World | 4 | Real OS/firmware vocabulary throughout |
| 3 | User Control and Freedom | 3 | No back-to-top, mobile nav doesn't collapse |
| 4 | Consistency and Standards | 2 | Sidebar vs body links: contradictory hover conventions |
| 5 | Error Prevention | 4 | Excellent: full-reorder-required writes, read-only locks |
| 6 | Recognition Rather Than Recall | 3 | Sidebar visible but no active state |
| 7 | Flexibility and Efficiency | 1 | No search, no shortcuts |
| 8 | Aesthetic and Minimalist Design | 3 | Faux-bold defect, font mismatch |
| 9 | Error Recovery | 4 | Troubleshooting maps real error strings to causes |
| 10 | Help and Documentation | 4 | The page is the help system |
| **Total** | | **30/40** | **Good** |

## Design Specificity Verdict
Content is bespoke (real error strings, real commands, real ABI paths, honest unverified-Linux admission). Visual chrome is a generic monochrome docs template with no BIOS/CLI-specific motif - arguably correct restraint given the brief, but the specificity lives in the words, not the design.

Deterministic scan: 0 findings across page.js/globals.css/layout.js (independently re-confirmed).
Visual evidence: screenshots at 1280px/768px/375px; no persistent overlay UI (Aside unavailable on Windows, gstack headless fallback used).

## Overall Impression
Strong honest content undermined by two concrete, independently-verified defects (mobile table overflow, faux-bold text) plus a UX gap undercutting the page's premise (sidebar with no active-position indicator).

## What's Working
1. Error-prevention/honesty copy (Elevation, Safety notes, Linux honesty note) - specific and trust-building.
2. Typographic restraint at layout level - 68ch measure, consistent rhythm, strict monochrome per brief.
3. Code-block overflow handling (`pre`) done correctly; the pattern the table should have copied.

## Priority Issues
- [P1] Capability table breaks mobile layout: no overflow-x wrapper on `table` (unlike `pre`), causes ~44px page-wide horizontal scroll at 375px (measured 419px scrollWidth vs 375px viewport, confirmed by both assessments).
- [P1] Faux-bold text: layout.js loads only weight 400 of Abyssinica SIL, CSS requests font-weight:700 for headings/strong (including safety-critical text) - browser is faux-bolding.
- [P2] Contradictory link hover conventions between sidebar nav links (underlined at rest, hover-only-darkens) and body links (underlined at rest, hover-removes).
- [P2] No active-section indicator in sidebar - zero client JS, no IntersectionObserver.
- [P3] No skip-to-content link - 12 tab stops before content on every load.

## Persona Red Flags
Jordan (first-timer): setup wall (Install/Build) before payoff, no lightweight try-it path.
Sam (accessibility): missing skip-link; table th lacks scope=col.
Casey (mobile): confirmed table overflow bug + tall uncollapsible nav block.

## Minor Observations
- nav/ul missing aria-label.
- Page ends on Troubleshooting instead of the stronger "Verified against real hardware" section - peak-end rule opportunity.
- .note box styling identical for different-stakes callouts (build caveat vs honesty admission).

## Questions to Consider
- If the brief was "just a sidebar nav, that's it," why no active-position indicator?
- Was Abyssinica SIL a deliberate choice or an unrevisited placeholder from the earlier rejected design?
