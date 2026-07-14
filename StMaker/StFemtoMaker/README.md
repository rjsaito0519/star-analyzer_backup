# StFemtoMaker naming rules

Canonical location for femto **species keys**, **particle keys**, and **channel names**.
When adding particles or channels, follow these rules and update `FemtoConfig` YAML accordingly.

## Species keys (`species` map in femto YAML)

- Format: `lower_snake_case`
- One key per candidate source registered in the event store
- Examples: `proton`, `anti_proton`, `kaon_plus`, `phi`
- Each species entry requires:
  - `builderType`: `track` or `resonance`
  - `particleKey`: builder dispatch key (see below)

## Particle keys (`particleKey` field)

- Selects builder logic inside generic `TrackPidBuilder` / `ResonanceBuilder`
- Phase 1 supported values: `proton` (track), `he4` (track, via `StNuclearIdHelper`), `deuteron` (track, via `StNuclearIdHelper`), `kaon_minus` (track, K⁻ bachelor), `phi` (resonance from KK)
- Background QA: `phi_rotation` (resonance builder dispatch → `BuildRotatedPhiCandidates`, stored as species `phi_rot`)
- Use PDG-inspired names; charge sign goes in species key when needed (`anti_proton`)

## Channel names

- Format: `{partA}_{partB}` using species keys
- Example: `phi_proton` with `partA: phi`, `partB: proton`; or `phi_he4` with `partB: he4`; or `phi_deuteron` with `partB: deuteron`
- Background channels (same `partB`, different `partA` / mass window): `phi_proton_signal`, `phi_proton_leftSB`, `phi_proton_rightSB`, `phi_rot_proton` (or `phi_he4_*` / `phi_rot_he4` for ⁴He; `phi_deuteron_*` / `phi_rot_deuteron` for d)
- Histogram suffixes use channel name: `hKstarSE_phi_proton`, `hCF_phi_proton`, `hKstarSEVsCent_phi_proton_signal`, `hPhiMKK_vs_KstarSE_phi_proton_signal` (TH3: M_KK × k* × cent9), etc.
- Mass window per channel: `signalMin` / `signalMax` on resonance `partA` invMass at pairing time (sidebands need no extra maker logic)

## Correlation function (CF)

- `StFemtoMaker` fills **SE/ME k* histograms only** (`hKstarSE_*`, `hKstarME_*`). It does not compute CF at `Finish()`.
- `hCF_*` may exist in hist YAML as empty shells; do not rely on CF in subjob or merged ROOT from Maker.
- **CF is computed in `checkHistAnaFemtoPhiProton.C`** from merged SE/ME after all events are summed:
  - SE/ME are rebinned first when `cfRebinFactor` > 1 in maker YAML (e.g. 100 k* bins / 5 → 20 CF bins).
  - **Legacy cent page (Page 20)**: project over `cfCent9Min`–`cfCent9Max` (default 2–8 ≈ 0–60%); columns signal / leftSB / rightSB / rot.
  - **Multi-slice CF (`cfCentSlices`)**: default 15 slices (cent9\_0…8 + pct\_0\_10…60). Per slice: signal, SB-L, SB-R, **SB-LR** (left+right histogram sum in checkHist, no new maker channel).
  - **Sideband-subtracted CF**: count-level `N_corr = N_sig - α N_SB` on SE and ME separately, then same CF formula (`sidebandSubtractAlpha`, default 1.0; `negativeBinPolicy: zero|skip`).
  - Bin-wise: `C(k*) = SE / (ME * seNorm/meNorm)` with `seNorm`, `meNorm` integrals over `normQMin`–`normQMax` from maker YAML.
  - Default norm region for `anaFemtoPhiProton`: **0.5–1.0 GeV/c** (high k*, where C→1 when SE≈ME).
- **PDF output** (one checkHist run, `share/figure/<anaName>/`):
  - QA: `{anaName}_checkHistAnaFemtoPhiProton_{jobid}.pdf` — Pages 1–19 + Page 20 + representative slices (`cfCentSlicesQaPdfInclude`, default pct\_0\_10/20/30).
  - CF: `{anaName}_checkHistAnaFemtoPhiProton_CF_{jobid}.pdf` — remaining slices (`cfPdfExcludeQaSlices: true` avoids duplicate).
  - **Per-slice layout (representative + CF PDF):** 4 columns (signal / SB-L / SB-R / SB-LR) × 4 rows (SE k*, ME k*, raw `CF_sig_raw`, sub `CF_sig_sub_SBL|SBR|SBLR` on row 4).
- Run QA on `*_merge.root`: `./script/singularity_checkHistAnaFemtoPhiProton.sh <merge.root> <mainconf>`.
- Implementation record: `analysisnote/YYYYMMDD/femto_cent_sb_cf_plan.md` (φ–p cent/SB/CF extension).

### Topic 3: k*-binned purity and CF_genuine (`anaFemtoPhi` unified)

- Maker fills **TH3** `hPhiMKK_vs_KstarSE/ME_<channel>_signal` (x = M_KK, y = k*, z = cent9) for signal channels only.
- checkHist (`checkHistAnaFemtoPhi.C`) post-processes per `cfCentSlices` slice:
  - scaled SE-ME subtraction on M_KK (per k* bin), Gaussian + constant background fit -> `lambda_sig(k*)`, `lambda_bkg = 1 - lambda_sig`.
  - `C_bkg` from ME mass shape: scale `hPhiMKK_vs_KstarME` in k* norm band, project signal mass window -> combinatorial CF (`cfBkgMode: me_mass`).
  - `C_genuine = 1 + [(C_meas-1) - lambda_bkg(C_bkg-1)] / lambda_sig` with `C_meas` = raw signal-channel CF.
  - `C_genuine` uncertainties use analytic propagation in `C_meas`, `C_bkg`, and `lambda_sig`; the `lambda_sig` error currently comes from the Gaussian-fit partial derivatives.
- YAML keys: `purityFitUseConstantBkg`, `purityFitGaussSigmaMin/Max`, `purityMinKstar/Max`, `purityMinEntriesPerBin`, `purityClampMin/Max`, `cfBkgMode`.
- QA PDF pages: `lambda_sig(k*)` and `C_meas` vs `C_genuine` for `cfCentSlicesQaPdfInclude` slices × all bachelor bases.
- Requires batch re-run after hist/Maker deploy so TH3 keys exist in merge ROOT.

### Phi–bachelor pair momentum angle QA (`anaFemtoPhi` unified)

- **Not used in CF** — QA histograms only; `FillSameEventPairs` / mixing unchanged.
- Angle θ: acos(p\_φ · p\_bachelor / |p\_φ||p\_bachelor|) in lab frame (distinct from KK opening angle).
- **Four φ QA stores / selections** for `phi_<bach>_signal` channels:
  - `hPhiPairMomAngle_phi_<bach>_signal` / `hPhiPairMomAngle_vs_MKK_*`: from `m_phiQaLoose` (passStage kinematics plus signal mass window, **no** `PassPairTofCut`); not stored in mixing pool.
  - `hPhiPairMomAngle_*_tofStrict` / `hPhiPairMomAngle_vs_MKK_*_tofStrict`: from `m_eventCandidates["phi"]` with `reso.betaGamma > 0` (both K daughters TOF-matched) plus signal mass window.
  - `hPhiPairMomAngle_vs_MKK_*_preMass`: all M_KK after DCA, track pT/eta, and nSigmaK cuts; no signal mass window, no KK opening-angle cut, no pair-rapidity cut.
  - `hPhiPairMomAngle_vs_MKK_*_preMass_tofStrict`: same preMass selection plus both K daughters pass the strict pair TOF requirement.
- Bachelor: existing `PassFemto*Cuts` candidates in `m_eventCandidates`.
- checkHist: `drawPhiBachelorPairAngleQaPage` — one PDF page per bachelor (p, d, t, ³He, ⁴He).
- Hist keys generated by `script/generate_hist_anaFemtoPhi.py` (30 histograms for 5 signal channels).

## Event mixing (ME statistics)

- Config: `config/cuts/mixing/mixing_<ana>.yaml` via mainconf `mixing:` key (`MixingConfig`).
- `mixingMode`: `randomSample` (default) or `bufferAll` (loop all buffer events × all pairs; Zhangwei-like).
- `maxMixedPairsPerEvent`: cap for **randomSample only** — max ME pairs **per event** (default 500; 0 = unlimited). Ignored by `bufferAll`.
- `mixBothDirections`: with `bufferAll`, also mix buffer `partA` with current `partB` (and vice versa).
- **Mixing bin:** `GetMixingBin(vz, cent9, psi2)` uses Vz bin × **cent9 as bin index** × EP bin. ME pairs only **different events** in the same bin (never same-event pairs; those are SE).
- **Centrality slices in checkHist:** `pct_0_10` etc. project cent9 ranges **after** fill; Maker mixing stays at cent9 resolution.
- Changing mixing requires a **new batch run**; cent-slice CF projection uses existing `hKstar*VsCent_*` in merge ROOT.
- **Rollout:** benchmark `bufferAll` on a short joblist before full catalog; check high-k* `C→1`, ME shape, job time, merge ROOT size.

## QA PDF pre/post layout (`checkHistAnaFemtoPhiProton.C`)

- Pre-cut pages are immediately followed by post-cut pages (event, track, proton femto, phi candidate).
- **KK pair (A):** `hOpeningAngle_Raw`, `hPairRapidity_vs_Pt`, … filled after strict TOF, before opening/rapidity; **AfterCuts** on pass.
- **Femto φ (B):** `hPhi_*_PreCut` at candidate push; `hPhi_*` + `hPhi_PtVsY_PostCut` in `FillCandidateQA`.
- **Proton:** `hP_*_PreFemtoCut` in `FillProtonPreFemtoQa`; post-femto in `FillProtonFemtoQa` / `FillCandidateQA`.
- Re-run analysis after Maker/hist changes so new ROOT keys exist in merge output.

## Centrality cuts (event level)

- Centrality YAML (`centrality:` in mainconf) may set optional **per-bin refMult_corr cap**:
  - `cent9MaxRefMultCorrBin: 8`, `cent9MaxRefMultCorr: 220` → reject cent8 events with refMult_corr > 220 (auau3p85fxt tail trim).
  - Applied in `CentralityHelper::AcceptCentBin` after `StRefMultCorr` bin assignment; requires analysis re-run to affect ROOT histograms.

## Mainconf and maker YAML

- StFemtoMaker analyses use mainconf key **`maker:`** (one file per StMaker; see `PHILOSOPHY.md`).
- Path: `config/maker/maker_<anaName>.yaml` (e.g. `maker_auau3p85fxt_anaFemtoPhiProton.yaml`).
- The same flat YAML holds both:
  - **φ resonance builder** keys (`PhiCutConfig`: `nSigmaKaon`, `minPairRapidity`, …)
  - **femto species/channels** keys (`FemtoConfig`: `speciesKeys`, `channel_*`, `proton*`, `rotation*`)
- `ConfigManager` loads `maker:` into `GetPhiCuts()` and `GetFemtoConfig()` from one file.

## YAML flat-key layout

`YamlParser` is flat; the femto section of maker YAML uses prefixed keys.

**`nChannels`** must be at least one greater than the highest `channel_N_*` index present (e.g. `channel_0` … `channel_4` requires `nChannels: 5`). If `nChannels` is too small, sideband and rotation channels are silently skipped at load time.

```yaml
speciesKeys: proton,phi
species_proton_builderType: track
species_proton_particleKey: proton
species_phi_builderType: resonance
species_phi_particleKey: phi

channelName: phi_proton
partA: phi
partB: proton
signalMin: 1.01
signalMax: 1.03
normQMin: 0.5
normQMax: 1.0
```

## Proton femto bachelor cuts (Zhangwei-like)

YAML keys on femto config (not `PIDCutConfig`): `protonChargeMode`, `protonMaxDca`, `protonMinPtPre`, `protonMinPtPair`, `protonMaxPtPair`, `protonMaxAbsEta`, `protonMaxAbsNSigma`, `protonMinNHitsFit`, `protonMinNHitsRatio`, `protonTofMomentumThreshold`, `protonMinMass2`, `protonMaxMass2`, `protonMinRapidityCm`, `protonMaxRapidityCm`. Applied in `PassFemtoProtonCuts` after generic `PassProtonCuts` + `IsProton`.

## K⁻ bachelor (`anaFemtoKaon`)

- **Species key:** `kaon_minus` with `builderType: track`, `particleKey: kaon_minus`.
- **Femto cuts** (`kaonMinus*` keys in maker YAML): mirror proton femto layout — DCA, nσ, nHits, TOF momentum threshold + m² window, pair pT, CM rapidity. TPC pre-cuts use `kaonMinusMaxDca` / `kaonMinusMaxAbsNSigma` (not `PhiCutConfig`).
- **TOF:** `IsKaon` (`PIDCutConfig`) at ID time; femto TOF rule via `kaonMinusTofMomentumThreshold` + `kaonMinusMin/MaxMass2`.
- **Channels (5, no background):** `kaon_minus_proton`, `kaon_minus_deuteron`, `kaon_minus_triton`, `kaon_minus_he3`, `kaon_minus_he4`.
- **QA hist prefix:** `hKm_`; k* keys `hKstarSE/ME_<channel>`.
- **CF:** computed in `checkHistAnaFemtoKaon.C` from merged SE/ME (Maker fills histograms only). Common `normQMin`/`normQMax` and `cfCentSlices` across channels (default: cent9 8–8, 7–8, 6–8, 5–8).
- **checkHist:** `checkHistAnaFemtoKaon.C` — K⁻ QA + bachelor-loop + inclusive CF + cent slices (no SB/rotation). Per cent slice: one page with 5-channel SE + norm-scaled ME k* overlay, then one CF page (3×2 grid).

## Unified multi-bachelor (`auau3p85fxt_anaFemtoKaon`)

- **anaName:** `auau3p85fxt_anaFemtoKaon` — one picoDst pass for K⁻–(p, d, t, ³He, ⁴He).
- **Species keys:** `kaon_minus`, `proton`, `deuteron`, `triton`, `he3`, `he4`.
- **Channels (5):** `kaon_minus_<bachelor>` only (purity=1.0, no sidebands).
- **Nuclear ID:** same as unified `anaFemtoPhi` (`nuclearid_auau3p85fxt_anaFemtoPhi.yaml`).
- **Maker YAML:** minimal `PhiCutConfig` keys only (`rapidityFrame`, event-plane, `maxNTr`) — no φ reconstruction keys.

## Rotation background

When `rotationEnabled: true`, species `phi_rot` with `particleKey: phi_rotation` fills rotated φ candidates (`rotationN` azimuth rotations per real KK pair; DCA/PID/TOF evaluated on real tracks). Pair with proton via channel `phi_rot_proton`.

## Agent / developer checklist

1. Add species block with unique species key
2. Implement or extend builder dispatch for new `particleKey` in `StFemtoMaker`
3. Add channel entry referencing existing species keys
4. Add histogram entries to `config/hist/hist_anaFemto*.yaml` using channel name suffix
5. Document new keys in this file

## ⁴He bachelor (`anaFemtoPhi4He`)

- **Nuclear ID** (`nuclearid:` YAML → `StNuclearIdHelper`): TPC nσ + `requireBestSpecies`. Set `m2_selection: false` to skip TOF m² at ID time.
- **Femto TOF rule** (`he4TofMomentumThreshold` in maker YAML): tracks with `|p| >= threshold` require TOF m² in `[he4MinMass2, he4MaxMass2]`. Set threshold high (e.g. `99`) to disable.
- **Kinematics** (maker `he4*` keys): primary window `he4MinPMom` / `he4MaxPMom`; pT limits `he4MinPtPre`–`he4MaxPtPre` (pre QA) and `he4MinPtPair`–`he4MaxPtPair` (femto cut).
- **QA histograms:** `hHe4_Mass2VsP` (m² 0–16) plus `hHe4_Mass2VsP_*_wide` (m² 0–20) for TOF diagnostics when fills are enabled.

## Deuteron bachelor (`anaFemtoPhiDeuteron`)

- **Nuclear ID** (`nuclearid:` YAML → `StNuclearIdHelper::IsDeuteron`): TPC nσ + `requireBestSpecies` (best among d/t/³He/⁴He). Set `m2_selection: false` to skip TOF m² at ID time (initial default matches `anaFemtoPhi4He`).
- **Femto TOF rule** (`deuteronTofMomentumThreshold` in maker YAML): tracks with `|p| >= threshold` require TOF m² in `[deuteronMinMass2, deuteronMaxMass2]`. Set threshold high (e.g. `99`) to disable.
- **Kinematics** (maker `deuteron*` keys): primary window `deuteronMinPMom` / `deuteronMaxPMom`; pT limits `deuteronMinPtPre`–`deuteronMaxPtPre` (pre QA) and `deuteronMinPtPair`–`deuteronMaxPtPair` (femto cut).
- **QA histograms:** `hDeuteron_Mass2VsP` (m² 0–16) plus `hDeuteron_Mass2VsP_*_wide` (m² 0–20) for TOF diagnostics when fills are enabled.

See also `docs/femto_track_sharing_concerns.md` for overlap risks when extending species.

## Unified multi-bachelor (`anaFemtoPhi`)

- **anaName:** `auau3p85fxt_anaFemtoPhi` — one picoDst pass for φ–(p, d, t, ³He, ⁴He).
- **Species keys:** `proton`, `deuteron`, `triton`, `he3`, `he4`, `phi`, `phi_rot` (rotation for proton only).
- **Channels (21):** `phi_<bachelor>`, `_signal`, `_leftSB`, `_rightSB` per bachelor; plus `phi_rot_proton`.
- **Nuclear ID:** `StNuclearIdHelper::IsTriton()` / `IsHe3()` with `requireBestSpecies: true` (bestType 1=triton, 2=he3).
- **QA hist prefixes:** `hP_`, `hDeuteron_`, `hTriton_`, `hHe3_`, `hHe4_`; k* keys `hKstarSE/ME_<channel>`.
- **checkHist:** `checkHistAnaFemtoPhi.C` — bachelor-loop QA; single PDF (no CF-only PDF).

