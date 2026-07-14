# Systematic uncertainty sources (common)

Use to audit Section 6 and Phase 2. Each row: **listed? quantified? dominant?**

## Event and track level

| Source | Typical variation | Notes |
|--------|-------------------|-------|
| Vertex cut (Vz, Vr) | Cut variation | Affects acceptance and centrality |
| Centrality definition | Alternate estimator, bin width | `StRefMultCorr` vs multiplicity |
| Pile-up / quality flags | Tighten / loosen event cuts | |
| Track quality (nHits, DCA, χ²) | ±1 hit, DCA ±20% | Efficiency vs purity trade-off |
| Primary vs secondary contamination | DCA, dE/dx band | |

## PID

| Source | Typical variation | Notes |
|--------|-------------------|-------|
| dE/dx band (nσ) | ±1σ, ±2σ | Species-dependent |
| TOF mass² window | Widen / narrow | Low-momentum dominance |
| dE/dx–TOF combined | 2D contour shift | |
| PID efficiency weighting | Embedding / MC | Closure test |
| Misidentification feed-down | Alternate PID set | |

## Acceptance and efficiency

| Source | Typical variation | Notes |
|--------|-------------------|-------|
| TPC acceptance | η, pT dependence | |
| Trigger / bias | Compare subsets | |
| Reconstruction efficiency | Embedding, MC truth | |
| Correction method | Weight vs unfold | |
| Centrality-dependent efficiency | Per cent9 bin | |

## Signal extraction / fit

| Source | Typical variation | Notes |
|--------|-------------------|-------|
| Fit range | ±10–20% width | |
| Background model | Alternate function | |
| Sideband definition | α, window width | Femto / resonance |
| Normalization region (k*, mass) | Shift norm band | Femto CF |
| Binning / rebinning | `cfRebinFactor` etc. | |

## Detector resolution

| Source | Typical variation | Notes |
|--------|-------------------|-------|
| Momentum scale | ±1–2% | |
| Mass resolution (resonances) | Width fixed vs floated | |
| k* resolution smearing | Compare data vs MC | Femto |

## Analysis-specific

| Source | Typical variation | Notes |
|--------|-------------------|-------|
| Mixed-event normalization | Buffer size, mixing mode | See femto checklist |
| Event-plane resolution | Sub-event, harmonic | Flow analyses |
| Material / decay feed-down | Vary feed-down correction | Strangeness |

## Reporting

- Separate **statistical** and **systematic** in tables and abstract.
- State combination method (quadrature, partial correlation).
- Flag correlated systematics that should not be added in quadrature blindly.
