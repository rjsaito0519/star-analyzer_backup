# Domain checklist — femto / correlation

Apply when `anaName` or physics goal involves femtoscopy, correlation functions, or pair analyses.

## Pair and channel definition

- [ ] Species keys and channel names follow [`femto-species-naming`](../../femto-species-naming.md) / `StFemtoMaker/README.md`
- [ ] Same-charge vs opposite-charge / quantum statistics addressed for identical particles
- [ ] Pair cuts documented: k* range, Δη, Δφ, shared-track removal, opening angle
- [ ] Signal vs sideband vs rotation channels defined (`signalMin`/`signalMax`, SB windows)

## Same-event / mixed-event

- [ ] SE pair source (on-the-fly vs stored candidates) stated
- [ ] Mixing mode: `randomSample` or `bufferAll` (`config/cuts/mixing/`)
- [ ] `maxMixedPairsPerEvent` (if `randomSample`) justified
- [ ] Mixing bin: Vz × cent9 × EP — same for SE and ME
- [ ] ME uses **different events only** (never same-event pairs in ME)

## Correlation function

- [ ] CF computed in checkHist (not Maker) — document macro (`checkHistAnaFemtoPhiProton.C` etc.)
- [ ] Normalization region (`normQMin`–`normQMax`) stated and justified (C → 1 at high k*)
- [ ] `cfRebinFactor` and bin edges documented
- [ ] Centrality slices (`cfCentSlices`) and projection ranges documented
- [ ] Sideband subtraction: `sidebandSubtractAlpha`, LR combination, `negativeBinPolicy`
- [ ] Low-statistics bins: treatment flagged (do not over-fit)

## Centrality

- [ ] cent9 convention per [`centrality-strefmultcorr`](../../centrality-strefmultcorr.md)
- [ ] `cfCent9Min`/`Max` or per-slice ranges stated
- [ ] QA: cent9 vs multiplicity monotonicity checked in QA PDF

## Systematics (femto-specific)

- [ ] Mixing buffer size / mode variation
- [ ] Norm region variation
- [ ] Sideband α and window variation
- [ ] Pair cut variation (k*, Δη, Δφ)
- [ ] PID on bachelor / resonance daughters
- [ ] k*-binned purity / `C_genuine` method if used (`purityFit*`, `cfBkgMode` YAML)

## Cross-checks (minimum)

- [ ] High-k* C ≈ 1
- [ ] SE/ME ratio vs k* and centrality
- [ ] Rotation or sideband null test
- [ ] Optional: `bufferAll` benchmark per `StFemtoMaker/README.md`

## Probing questions

- Why this norm region and not adjacent k*?
- Is residual correlation at low k* from mixing or physics?
- Does cent slice have enough pairs for SB subtraction?
- Are femto cuts applied symmetrically to SE and ME paths?
