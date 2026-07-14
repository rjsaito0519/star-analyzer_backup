# Section examples (placeholders only)

**Do not copy numeric values into a real note unless the user confirms them.** These illustrate tone and structure.

## Abstract (example)

We measure the φ–p correlation function \(C(k^*)\) in Au+Au collisions at \(\sqrt{s_{NN}} = 3.85\) GeV using the STAR TPC and TOF. Same-event and mixed-event pairs are formed in centrality, vertex, and event-plane bins. After sideband subtraction and normalization in \(0.5 < k^* < 1.0\) GeV/\(c\), we extract [要確認] over centrality bins 0–60%. Systematic uncertainties from PID, mixing, and sideband normalization are evaluated via cut variations [要確認].

## Event selection (example table row)

| Cut | Value | Justification |
|-----|-------|---------------|
| \|Vz\| | < 30 cm | TPC acceptance; matches centrality calibration |
| refMult_corr | per YAML cent9 caps | Remove pile-up / edge events per `StRefMultCorr` |

## Systematic row (example)

| Source | Variation / method | Magnitude | Dominant? |
|--------|-------------------|-----------|-----------|
| TOF PID | ±2σ nσ shift | [要確認]% on yield | yes / no |
| Mixing buffer | `randomSample` vs `bufferAll` | [要確認] on \(C(k^*)\) at low \(k^*\) | |

## Conclusion (example — appropriately cautious)

Within statistical and quoted systematic uncertainties, the measured \(C(k^*)\) in 0–30% centrality is consistent with [要確認] at \(k^* < 0.2\) GeV/\(c\). A definitive statement on [physics claim] requires [要確認: e.g. improved embedding, larger statistics].
