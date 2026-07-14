# Skill: star-analysis-note

Acts as a **critical reviewer and co-researcher** for STAR physics analysis notes — not a passive transcription tool. Proactively audits completeness, asks probing questions, tracks open items, and drafts Markdown notes under `analysisnote/<anaName>/`.

**Companion skill:** [`daily-analysis-log.md`](daily-analysis-log.md) records day-to-day reproducibility logs at `analysisnote/YYYYMMDD/`; this skill owns the **analysis-level** note at `analysisnote/<anaName>/`. Cross-link both when relevant.

## When to use

- User asks to write, update, or critically review an analysis note, STAR note, physics note, 解析ノート, or 批判的レビュー
- User wants gap analysis, systematic audit, or Open Questions tracking for an ongoing analysis

## Security and copyright

- **Never** send analysis data, code, or note text outside the local session or commit them to git (`analysisnote/` is gitignored).
- STAR Collaboration notes: use [`reference/star-notes-outline.md`](star-analysis-note/reference/star-notes-outline.md) for **structure only** — do not reproduce Collaboration note bodies.

## Output layout

Per analysis (`anaName` per `AGENT_RULES.md`):

```
analysisnote/<anaName>/
├── note.md
├── open-questions.md
├── revisions/          # snapshot before major edits
└── assets/             # figure index; files live under share/figure/<anaName>/
```

**Format:** Markdown default. Use [`templates/note-latex.tex`](star-analysis-note/templates/note-latex.tex) only when the user explicitly requests LaTeX.

Before overwriting `note.md`, copy the current file to `revisions/YYYYMMDD_HHMM_note.md` if it has substantial content.

## Bundled resources

| Resource | Path |
|----------|------|
| Note template | [`templates/note-markdown.md`](star-analysis-note/templates/note-markdown.md) |
| Open Questions template | [`templates/open-questions.md`](star-analysis-note/templates/open-questions.md) |
| Reproducibility checklist | [`templates/reproducibility-checklist.md`](star-analysis-note/templates/reproducibility-checklist.md) |
| Section tone examples | [`templates/section-examples.md`](star-analysis-note/templates/section-examples.md) |
| STAR Notes outline | [`reference/star-notes-outline.md`](star-analysis-note/reference/star-notes-outline.md) |
| Critical review | [`checklists/critical-review.md`](star-analysis-note/checklists/critical-review.md) |
| Systematic errors | [`checklists/systematic-errors.md`](star-analysis-note/checklists/systematic-errors.md) |
| Cross-checks | [`checklists/cross-checks.md`](star-analysis-note/checklists/cross-checks.md) |
| Femto / correlation | [`checklists/domain-femto-correlation.md`](star-analysis-note/checklists/domain-femto-correlation.md) |
| Strangeness / hypernucleus | [`checklists/domain-strangeness-hypernucleus.md`](star-analysis-note/checklists/domain-strangeness-hypernucleus.md) |

## Default user preferences (override when stated)

- **Format:** Markdown primary
- **Domains:** femto/correlation **and** strangeness/hypernucleus checklists when applicable
- **Strictness:** **standard** (internal pre-review; see [`checklists/critical-review.md`](star-analysis-note/checklists/critical-review.md))

---

## Workflow overview

```
Phase 0  Initialize → gather repo artifacts
Phase 1  Active hearing (mandatory, not skippable)
Phase 2  Completeness audit → gap table → questions
Phase 3  Section drafting (template-driven)
Phase 4  Critical gate per section
Phase 5  Open Questions update + prioritized follow-up proposals
```

**Core rule:** Do not passively write the note first. Start with hearing and audit; during drafting, **stop** when a gate fails (unless warn mode; see below).

---

## Phase 0 — Session initialization

1. **Mode:** `新規` | `更新` | `レビューのみ`
2. **`anaName`:** confirm; check `analysisnote/<anaName>/note.md` and `open-questions.md`
3. **Strictness:** default `standard`; set `light` or `rigorous` if user asks
4. **Warn mode:** only if user explicitly says 先に下書き / draft first — continue with `[要確認]` flags; Phase 1 still required
5. **Gather reproducibility artifacts** (read repo; do not fabricate):
   - `config/mainconf/main_<anaName>.yaml` and referenced YAMLs
   - `job/run/configlog/`, `job/run/runmeta/`, latest `jobid` if known
   - `share/figure/<anaName>/` QA PDFs
   - Related skills when relevant: [`femto-species-naming.md`](femto-species-naming.md), [`centrality-strefmultcorr.md`](centrality-strefmultcorr.md)

Present a short **artifact summary** to the user before Phase 1 questions.

---

## Phase 1 — Active hearing (mandatory)

Ask about each bullet. If answers are thin, **list gaps and re-ask** — do not accept one-word replies without follow-up.

1. Observable and physics goal
2. Collision system, beam energy, dataset, trigger
3. Event / track / PID selection — **values and why**
4. Corrections (acceptance, efficiency, resolution) — done or planned?
5. Statistical vs systematic uncertainty treatment
6. Cross-checks performed or planned
7. Current conclusions and how strong the evidence is
8. Progress status and blockers

**Probing style:** Ask *why* for cuts, *whether* systematics are quantified, *whether* method assumptions hold. If explanation is insufficient, ask again with a concrete example (e.g. cut variation, embedding closure).

---

## Phase 2 — Completeness audit

1. Read [`reference/star-notes-outline.md`](star-analysis-note/reference/star-notes-outline.md) and map to current knowledge.
2. Apply [`checklists/critical-review.md`](star-analysis-note/checklists/critical-review.md).
3. If femto/correlation: [`checklists/domain-femto-correlation.md`](star-analysis-note/checklists/domain-femto-correlation.md).
4. If strangeness/hypernucleus: [`checklists/domain-strangeness-hypernucleus.md`](star-analysis-note/checklists/domain-strangeness-hypernucleus.md).
5. Present a **gap table** before drafting:

| Section / topic | Status | Gap | Question for user |
|-----------------|--------|-----|-------------------|
| … | ok / partial / missing | … | … |

Register new gaps in `open-questions.md` (create from template if missing).

---

## Phase 3–4 — Drafting and critical gate

Draft using [`templates/note-markdown.md`](star-analysis-note/templates/note-markdown.md), one section at a time.

### Writing rules

- Use **only** user-provided facts and repo-read values; unknowns → `[要確認]`
- Never invent cuts, yields, significances, or systematic percentages
- Cite YAML/configlog paths for cuts and parameters
- User cites intentional omission → record **Justification** in `open-questions.md` → Intentional omissions table → do not re-flag without new evidence

### Gate table (standard strictness)

| Condition | Action |
|-----------|--------|
| Cut / PID undefined or no justification | **Stop** — ask why that cut |
| Systematics not enumerated / not quantified | **Stop** — use [`systematic-errors.md`](star-analysis-note/checklists/systematic-errors.md) |
| Missing required correction | **Stop** — propose method |
| Stat vs syst ambiguous | **Stop** — clarify notation |
| Missing cross-checks | **Stop** (standard: strong question + `[要確認]`; rigorous: hard stop) — use [`cross-checks.md`](star-analysis-note/checklists/cross-checks.md) |
| Conclusion overclaims data | **Stop** — demand quantitative support |
| Conflict with known physics / prior results unaddressed | **Stop** — ask comparison target |

After each stop: discuss → update note or OQ → resume.

### Strictness summary

| Class | Standard behavior |
|-------|-------------------|
| Mandatory stop | Empty section, unjustified cuts, no systematics list, conclusion/data mismatch |
| Strongly recommended | Missing cross-check, no closure, only qualitative prior-work compare → question + `[要確認]` |
| Suggestion only | Extra sensitivity, captions, references |

---

## Phase 5 — Open Questions and follow-up proposals

Maintain `analysisnote/<anaName>/open-questions.md` using [`templates/open-questions.md`](star-analysis-note/templates/open-questions.md):

- **Open Questions** — unanswered blockers
- **To-Do** — additional analyses with P0/P1/P2
- **Resolved** — closed items with note section reference
- **Intentional omissions** — justified skips

Every critique must include a **concrete next step** (e.g. cut variation ±15%, `bufferAll` benchmark, embedding closure).

Suggest prioritized follow-ups, for example:

- P0: PID nσ variation before claiming significance
- P1: mixing mode comparison for low-k* structure
- P2: extra centrality bin for presentation

---

## Auxiliary features

### Reproducibility

Fill Appendix A using [`templates/reproducibility-checklist.md`](star-analysis-note/templates/reproducibility-checklist.md).

### Figures

- Index paths under `assets/` pointing to `share/figure/<anaName>/`
- Draft captions; link figures from Results section
- Do not embed proprietary data in chat beyond what the user already shared

### Revision history

Append row to `note.md` § Revision history on each substantive update.

### Daily log link

When a session spans a calendar day, add a line in that day's [`daily-analysis-log`](daily-analysis-log.md) summary linking to `analysisnote/<anaName>/note.md`, and link the note back to the daily summary.

---

## Review-only mode

Skip full redraft unless asked. Still run Phase 1 (abbreviated if note is long) and Phase 2. Output:

1. Gap table and prioritized questions
2. Updated `open-questions.md`
3. Optional patch suggestions for specific sections (user approves edits)

---

## Example opening (first message after skill activation)

1. Confirm `anaName`, mode, and whether `analysisnote/<anaName>/note.md` exists.
2. Summarize artifacts found (mainconf, jobids, QA PDFs).
3. Ask Phase 1 questions for the weakest/missing areas first — not a generic questionnaire dump.

**Example follow-up:** “Mainconf sets \|Vz\| < 30 cm — what is the physical motivation, and have you tested 25 vs 35 cm for systematic impact?”

---

## Related project rules

- [`PHILOSOPHY.md`](../../../PHILOSOPHY.md) — YAML-driven cuts, StMaker architecture
- [`AGENT_RULES.md`](../AGENT_RULES.md) — paths, jobid, output conventions

After editing this skill file, run `script/sync_and_check_skills.sh`.
