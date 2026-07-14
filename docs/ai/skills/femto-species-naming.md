# Femto species and channel naming

Use when adding femto particles, channels, or histograms in `StFemtoMaker`.

## Canonical rules location

**`StMaker/StFemtoMaker/README.md`** — source of truth for:

- species keys (`lower_snake_case`)
- `particleKey` / `builderType` conventions
- channel naming (`partA_partB`)
- flat femto YAML key layout

Headers `include/FemtoCandidate.h` and `include/cuts/FemtoConfig.h` point to this file; do not duplicate rules elsewhere.

## Required steps when extending femto

1. Read `StMaker/StFemtoMaker/README.md` and follow naming exactly.
2. Update `config/maker/maker_<anaName>.yaml` (femto section) with new species/channel keys.
3. Extend builder dispatch in `StFemtoMaker` for new `particleKey` values (generic builders, not analysis-specific classes).
4. Add histogram names using channel suffix: `hKstarSE_<channel>`, `hKstarME_<channel>`, optional empty `hCF_<channel>` shell in hist YAML.
   - Topic 3 (`anaFemtoPhi` unified): `hPhiMKK_vs_KstarSE/ME_<channel>_signal` (TH3: M_KK × k* × cent9); generate via `script/generate_hist_anaFemtoPhi.py`.
   - Phi-bachelor momentum angle QA (`anaFemtoPhi`): `hPhiPairMomAngle_<channel>_signal` (+ `_tofStrict`), `hPhiPairMomAngle_vs_MKK_<channel>_signal` (+ `_tofStrict`, `_preMass`, `_preMass_tofStrict`); CF unchanged; see README Topic "Phi-bachelor pair momentum angle QA".
5. CF for QA: computed in checkHist from merged SE/ME (not in Maker output).
   - Per-analysis macros: `checkHistAnaFemtoPhiProton.C`, `checkHistAnaFemtoPhi4He.C`, `checkHistAnaFemtoPhiDeuteron.C` (QA + separate CF PDF).
   - `cfRebinFactor`, `cfCent9Min`/`cfCent9Max` (legacy Page 20).
   - `cfCentSlices` (default 15 slices: cent9\_0…8 + pct\_0\_10…60), `cfCentSlicesQaPdfInclude`, `cfPdfExcludeQaSlices`.
   - SB-LR: project `phi_<bachelor>_leftSB` + `phi_<bachelor>_rightSB`, then `combineSidebandLR` (no maker channel).
   - Sideband subtract (count-level): `sidebandSubtractAlpha`, `sidebandAlphaMode`, `negativeBinPolicy`; sub CF `CF_sig_sub_SBL|SBR|SBLR`.
   - Legacy per-species analyses output two PDFs: QA + `*_CF_{jobid}.pdf`.
   - **Topic 3 (unified `checkHistAnaFemtoPhi.C`)**: k*-binned `lambda_sig` from MKK fit; `C_bkg` from ME mass (`cfBkgMode: me_mass`); `C_genuine` formula in README Topic 3 section; purity YAML keys in maker femto block.
6. ME statistics: `mixingMode` (`randomSample`|`bufferAll`), `maxMixedPairsPerEvent` (per-event cap, randomSample only), `mixBothDirections`, `bufferSize` in mixing YAML (`MixingConfig`). Re-batch after mixing changes.
7. Update `config/hist/hist_anaFemto*.yaml` and checkHist macro pages if QA changes.
8. Document new keys in `StMaker/StFemtoMaker/README.md`.

## Guardrails

- Do not modify `StPhiMaker` / `StLambdaMaker` production behavior for femto work.
- Keep cuts in YAML; no hardcoded thresholds in maker code.
- k* uses PRF relative momentum (`|q*|/2` after pair boost), not lab 3-momentum subtraction.

## Related docs

- `docs/femto_track_sharing_concerns.md` — overlap / sharing risks
- `analysisnote/YYYYMMDD/femto_cent_sb_cf_plan.md` — φ–p cent/SB/CF extension plan record (when present)
- `.cursor/skills/add-new-analysis/SKILL.md` — general analysis entry pattern
