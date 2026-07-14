# [Analysis title — 要確認]

**anaName:** `<anaName>`
**Author:** [要確認]
**Date:** YYYY-MM-DD
**Status:** draft | internal-review | frozen
**Keywords:** [要確認]
**Category:** [要確認]
**Type:** internal-note | pre-publication | collaboration-draft

---

## Abstract

[One paragraph: observable, collision system, beam energy, detectors, method, headline result. Use only confirmed facts; mark unknowns as [要確認].]

---

## 1. Dataset and trigger

| Item | Value | Source / note |
|------|-------|---------------|
| Collision system | [要確認] | |
| Beam energy | [要確認] | |
| Dataset / production | [要確認] | |
| Trigger | [要確認] | |
| Luminosity / statistics | [要確認] | |

---

## 2. Event selection

| Cut | Value | Justification |
|-----|-------|---------------|
| | | |

**YAML / config reference:** `config/mainconf/main_<anaName>.yaml` → [要確認]

---

## 3. Track selection

| Cut | Value | Justification |
|-----|-------|---------------|
| | | |

---

## 4. Particle identification (dE/dx, TOF)

| Species | dE/dx criterion | TOF criterion | Efficiency note |
|---------|-----------------|---------------|-----------------|
| | | | |

---

## 5. Corrections

### 5.1 Acceptance

[要確認]

### 5.2 Efficiency

[要確認]

### 5.3 Resolution

[要確認]

---

## 6. Systematic uncertainties

| Source | Variation / method | Magnitude | Dominant? |
|--------|-------------------|-----------|-----------|
| | | [要確認] | |

See also: [open-questions.md](../open-questions.md) for unresolved systematics.

---

## 7. Fit / signal extraction

[Method, range, background model, goodness-of-fit. No invented numbers.]

---

## 8. Model / theory comparison

[要確認]

---

## 9. Results and discussion

[Figures: see `assets/` and `share/figure/<anaName>/`.]

---

## 10. Conclusions

[Must be supported by stated data and uncertainties. Flag over-claims as [要確認].]

---

## Appendix A — Reproducibility

| Item | Path / value |
|------|--------------|
| mainconf | `config/mainconf/main_<anaName>.yaml` |
| jobid | [要確認] |
| configlog | `job/run/configlog/config_<anaName>_<jobid>.txt` |
| runmeta | `job/run/runmeta/runmeta_<anaName>_<jobid>.json` |
| merge ROOT | `rootfile/<anaName>/` |
| QA PDF | `share/figure/<anaName>/` |
| Macro | [要確認] |
| Git commit (submit) | from runmeta |

Full checklist: [reproducibility-checklist.md](reproducibility-checklist.md).

---

## Appendix B — Open Questions / To-Do

Maintained in [open-questions.md](../../analysisnote/<anaName>/open-questions.md) (or inline below if preferred).

---

## Revision history

| Date | Author | Summary |
|------|--------|---------|
| YYYY-MM-DD | [要確認] | Initial draft |
