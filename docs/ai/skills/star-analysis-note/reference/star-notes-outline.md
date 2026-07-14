# STAR Notes — structural outline (reference only)

**Copyright / usage:** This file describes the **section structure and metadata fields** commonly used in STAR Collaboration analysis notes. It does **not** reproduce text from any Collaboration note. Do not fetch, paste, or paraphrase proprietary note bodies. Use this outline only to organize the user's own `analysisnote/<anaName>/note.md`.

## Metadata block

| Field | Description |
|-------|-------------|
| Title | Analysis title |
| Author(s) | Note author(s) |
| Date | Note date or last major revision |
| Keywords | Physics topics, systems, observables |
| Category | e.g. femtoscopy, strangeness, flow |
| Type | internal, public, preliminary |
| anaName | Repository analysis identifier |
| Status | draft / internal-review / frozen |

## Abstract

Single concise paragraph covering:

- Physics observable and motivation
- Collision system and beam energy
- Detectors and data sample
- Analysis method (one sentence)
- Principal result with uncertainty type (stat / syst)

## Body sections (standard order)

1. **Introduction / motivation** — physics question, prior work (user's own words + citations)
2. **Detector and dataset** — STAR subsystems used, trigger, data production, integrated luminosity
3. **Event selection** — cuts table with justification
4. **Track selection** — quality cuts, acceptance
5. **Particle identification** — dE/dx, TOF, Bayesian / combined methods
6. **Corrections** — acceptance maps, efficiency, momentum resolution, centrality / EP
7. **Systematic uncertainties** — source table with variations and magnitudes
8. **Analysis details** — channel-specific (e.g. femto pairing, invariant mass fit, flow harmonics)
9. **Results** — figures with captions; quantitative tables
10. **Discussion** — comparison to models / other measurements
11. **Summary and conclusions** — claims matched to evidence strength

## Appendices (recommended)

- **Reproducibility** — configs, jobids, software versions
- **Cut table** — full YAML-aligned listing
- **Open points** — link to `open-questions.md`
- **Revision history**

## Quality bar (what STAR Notes aim for)

- Another analyst can reproduce the procedure from the note + cited configs
- Systematics are enumerated, not hand-waved
- Figures are self-contained with axis labels, units, and systematic bands where applicable
- Conclusions state what is **not** yet established

Map this outline to [`templates/note-markdown.md`](../templates/note-markdown.md) when drafting.
