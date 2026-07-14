# StRefMultCorr (vendored centrality)

STAR reference-multiplicity centrality correction and binning. Vendored under `StRoot/StRefMultCorr/` → `lib/libStRefMultCorr.so`. See [`PROVENANCE.md`](PROVENANCE.md) for copy source and run-range notes.

**Agents and developers:** read this file before changing centrality YAML, `CentralityHelper`, cent-dependent histograms, or checkHist cent projections.

## What the class provides

- **Corrected multiplicity** `refMult_corr` (z-vertex and optional luminosity dependence)
- **Centrality bins:** `getCentralityBin9()` (9 bins) and `getCentralityBin16()` (16 bins)
- **Pileup rejection:** `passnTofMatchRefmultCut` / `isPileUpEvent`
- **Peripheral reweighting:** `getWeight()` (shape × trigger efficiency)

Multiplicity **mode** is set at construction (`refmult`, `fxtmult`, …). This repo uses:

| `centrality.mode` (YAML) | `StRefMultCorr` name | Typical use |
|--------------------------|----------------------|-------------|
| `refmult` | `refmult` | Collider (e.g. 19 GeV Au+Au) |
| `fxtmult` | `fxtmult` | Fixed-target / BES-II (e.g. 3.85 GeV FXT) |

Raw multiplicity passed to `initEvent` must match the mode (see `StFemtoMaker` / `StPhiMaker`: `fxtMult()` when mode is `fxtmult`, else `refMult()`).

## cent9 bin index convention (read this first)

**Bin index is not centrality percentile.** In STAR `StRefMultCorr`:

- **Larger cent9 index → more central** (higher multiplicity)
- **cent9 = 0 → most peripheral** (70–80%)
- **cent9 = 8 → most central** (0–5%)

Official table from `StRefMultCorr.h`:

| cent9 bin | Centrality (%) | cent16 bin (for reference) |
|-----------|----------------|----------------------------|
| 0 | 70–80 | 14–15 |
| 1 | 60–70 | 12–13 |
| 2 | 50–60 | 10–11 |
| 3 | 40–50 | 8–9 |
| 4 | 30–40 | 6–7 |
| 5 | 20–30 | 4–5 |
| 6 | 10–20 | 2–3 |
| 7 | 5–10 | 1 |
| 8 | 0–5 | 0 |

`CentralityHelper::Cent9ToPercentile(cent9)` returns the **mid-percentile** of each bin (e.g. cent9=8 → 2.5%, cent9=0 → 77.5%). Defined in `StMaker/common/CentralityHelper.cxx`.

### Common mistake

| Goal | Wrong (bin index ≠ percentile) | Correct cent9 range |
|------|----------------------------------|---------------------|
| 0–60% centrality (exclude 60–80%) | cent9 **0–6** (≈10–80% peripheral) | cent9 **2–8** |
| Most central 20% only | cent9 0–1 | cent9 **7–8** |
| Peripheral 40% | cent9 6–8 | cent9 **0–3** |

Phrase **“0–60% centrality”** means impact-parameter / percentile from **0% (most central) to 60%** — i.e. drop bins **0** (70–80%) and **1** (60–70%), keep **2–8**.

## Framework integration

### Config

- **mainconf:** `centrality: cuts/centrality/centrality_<anaName>.yaml`
- **Keys:** `CentralityCutConfig` in `include/cuts/CentralityCutConfig.h`

| YAML key | Meaning |
|----------|---------|
| `enabled` | Use `CentralityHelper` |
| `mode` | `refmult` or `fxtmult` |
| `acceptedCentBins` | Comma-separated **cent9 indices** to keep (empty = all 0–8) |
| `cent9MaxRefMultCorrBin` | Apply refMult_corr cap only when `cent9` equals this bin (e.g. `8`) |
| `cent9MaxRefMultCorr` | Reject if `refMult_corr` > this value in that bin (disabled if bin < 0 or max ≤ 0) |
| `rejectBadRun`, `rejectPileup`, `useWeight`, `fillCentralityQA` | Standard StRefMultCorr workflow |

Example (auau3p85fxt FXT): `config/cuts/centrality/centrality_auau3p85fxt_anaPhi.yaml`.

### Event-loop order (`CentralityHelper`)

Per event in Makers (`StFemtoMaker`, `StPhiMaker`, `StLambdaMaker`, …):

1. `CheckBadRun`
2. Event cuts (Vz, Vr, …)
3. `CheckPileup(rawMult, nBTOFMatch, vz)`
4. `ComputeBins` → `cent9`, `cent16`, `refMult_corr`, weight via `StRefMultCorr::initEvent` + `getCentralityBin9()`
5. Optional QA: `hCentralityRaw`, `hRefMultCorr`, … (**before** accept)
6. **`AcceptCentBin(cent9, refMult_corr)`** — `acceptedCentBins` then optional `cent9MaxRefMultCorr` trim
7. If rejected → return; **no tracks / pairs / femto fills** for that event
8. If accepted → `hCentrality` (weighted), physics loops, `hKstarSEVsCent` with Y = cent9 index

**checkHist cent projections do not re-apply** `AcceptCentBin` or refMult_corr cuts; they only sum existing `hKstar*VsCent` bins. Maker cuts are baked into ROOT at batch time.

### Histogram axes

- `hCentrality`, `hCentralityRaw`, centrality QA vs cent9: **X or Y = cent9 bin index** (0 left / peripheral, 8 right / central in typical 1D plots)
- `hKstarSEVsCent_*`, `hKstarMEVsCent_*`: **Y-axis = cent9** (see hist YAML `Cent9` axis)

### Femto checkHist cent-slice CF

In `maker_*.yaml` (FemtoConfig):

```yaml
cfCent9Min: 2
cfCent9Max: 8
```

`checkHistAnaFemtoPhiProton.C` projects `hKstar*VsCent` over cent9 ∈ [min, max] and builds CF. Default **2–8 = 0–60% centrality** (see table above). Change min/max in YAML, not in the macro.

## Build and load

- Build: `make` produces `lib/libStRefMultCorr.so` from this directory.
- Run macros load **`libStRefMultCorr.so` from this repo** before analysis Makers (see `run_anaPhi.C`, `run_anaFemtoPhiProton.C`).
- Do not mix with `$STAR/lib/libStRefMultCorr.so` (duplicate symbols). See `PROVENANCE.md`.

## QA and debugging

- **Integrated:** `hCentrality`, `hRefMultCorr_vs_Cent9`, pileup 2D plots in Phi / Lambda / Femto checkHist PDFs.
- **Standalone picoDst:** `script/checkCentrality.sh` / `script/singularity_checkCentrality.sh`.
- **Reference macro:** `checkStRefMultCorr.C` in this directory.

## Related docs

- [`PROVENANCE.md`](PROVENANCE.md) — vendored source, FXT run-range extension
- [`docs/REFERENCE.md`](../../docs/REFERENCE.md) — Centrality section (summary; this README is canonical for bin conventions)
- [`docs/ai/skills/centrality-strefmultcorr.md`](../../docs/ai/skills/centrality-strefmultcorr.md) — agent workflow
- [`analysisnote/20260521/centrality_qa_histograms.md`](../../analysisnote/20260521/centrality_qa_histograms.md) — QA histogram design notes
