# Skill: centrality-strefmultcorr

Use when editing centrality selection, `CentralityHelper`, centrality YAML, cent-dependent histograms, mixing bins vs cent9, or checkHist / femto cent-slice CF (`cfCent9Min` / `cfCent9Max`).

## Required reading (in order)

1. **[`StRoot/StRefMultCorr/README.md`](../../../StRoot/StRefMultCorr/README.md)** — **source of truth** for cent9/cent16 bin indices, percentile mapping, and common mistakes (e.g. 0–60% → cent9 **2–8**, not 0–6).
2. [`PROVENANCE.md`](../../../StRoot/StRefMultCorr/PROVENANCE.md) — vendored code, FXT run coverage.
3. [`include/cuts/CentralityCutConfig.h`](../../../include/cuts/CentralityCutConfig.h) and [`StMaker/common/CentralityHelper.h`](../../../StMaker/common/CentralityHelper.h) — framework API.

## Workflow checklist

1. **Confirm bin convention** before choosing `acceptedCentBins`, `cfCent9Min`/`Max`, or histogram cent axes. Larger cent9 index = more central.
2. **YAML changes** go in `config/cuts/centrality/centrality_<anaName>.yaml` (referenced from mainconf `centrality:`). No hardcoded centrality thresholds in Maker code.
3. **Maker event loop:** cuts run via `CentralityHelper::AcceptCentBin` **before** track/pair loops. Rejected events do not fill `hCentrality` or physics histograms; some pre-accept QA (`hCentralityRaw`, `hRefMultCorr`) may still fill.
4. **`cent9MaxRefMultCorr`:** applies only when `cent9 == cent9MaxRefMultCorrBin` (typically bin 8). Requires **batch re-run** to affect ROOT; checkHist projection does not apply this cut retroactively.
5. **checkHist cent-slice CF:** only projects existing `hKstar*VsCent`; set `cfCent9Min`/`cfCent9Max` in `config/maker/maker_<anaName>.yaml` (FemtoConfig).
6. **Mode:** `fxtmult` vs `refmult` must match analysis (FXT 3.85 GeV vs collider). Wrong mode → wrong bins and breaks Phi CM rapidity auto-frame when `rapidityFrame: auto`.
7. **QA:** after logic changes, verify centrality pages in checkHist PDF (1b–1c for Phi/Femto) and/or run `script/singularity_checkCentrality.sh` on picoDst.

## Do not

- Remap cent9 bin order in Makers (store `getCentralityBin9()` as-is).
- Assume cent9 bin number equals centrality percentile (bin 0 ≠ 0–5% central).
- Duplicate the full bin table outside `StRoot/StRefMultCorr/README.md`; link to it instead.

## Related skills

- [`reuse-star-stroot.md`](reuse-star-stroot.md) — how `StRefMultCorr` was vendored; use when adding new `StRoot/` packages
- [`femto-species-naming.md`](femto-species-naming.md) — `cfCent9Min`/`Max` for femto CF pages
- [`stmaker-add-histograms.md`](stmaker-add-histograms.md) — centrality QA histograms in Makers
