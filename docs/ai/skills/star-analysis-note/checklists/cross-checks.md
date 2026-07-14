# Cross-checks and validation

Prioritize by analysis type. Register missing checks in `open-questions.md` with P0/P1/P2.

## Universal cross-checks

| Check | Purpose | Priority if missing |
|-------|---------|---------------------|
| Cut variation (±10–20% on key cuts) | Expose sensitivity | P0 for publication |
| Split dataset (run period, field polarity) | Stability | P1 |
| Compare independent subsample statistics | Bin-to-bin fluctuation sanity | P1 |
| MC / embedding closure | Efficiency and PID | P0 when corrections applied |
| Null / sideband / rotation test | Background shape | P0 for resonance / femto |
| Reproduce known reference plot | Regression after code change | P1 |

## Femto / correlation (see also `domain-femto-correlation.md`)

| Check | Purpose |
|-------|---------|
| SE vs ME shape at high k* (C → 1) | Normalization sanity |
| `randomSample` vs `bufferAll` mixing | ME statistics bias |
| Buffer size scan | Residual correlation |
| Same-centrality vs mixed-centrality projection | Centrality mixing artifact |
| Sideband α variation | Combinatorial subtraction |
| Like-sign or identical-particle consistency | Quantum statistics / pair cuts |
| k*-binned purity closure | Genuine CF extraction |

## Strangeness / hypernucleus (see also `domain-strangeness-hypernucleus.md`)

| Check | Purpose |
|-------|---------|
| Mixed-event background vs like-sign | Combinatorial level |
| Mass window scan | Signal stability |
| Lifetime / DCA cut variation | Secondary vs weak decay |
| Matter vs antimatter comparison | CP / calibration |
| Known mass peak position (Λ, K0S, etc.) | Scale calibration |
| Embedding efficiency closure | Yield correction |

## Closure test definition

A closure test passes when applying the full analysis chain to MC/embedding reproduces truth within quoted uncertainties. Document:

- Input MC sample
- Whether detector effects are included
- Pass/fail criterion
- Action if fail

## Suggested priority wording for To-Do

- **P0:** Required before claiming discovery or publication-ready result
- **P1:** Required before internal-review freeze
- **P2:** Strengthens paper response to referees
