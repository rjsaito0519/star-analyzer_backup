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
- Requires `legacyCfPagesEnabled: true`. Batch re-run after hist/Maker deploy so TH3 keys exist in merge ROOT.

### kstarMassFitCF (`anaFemtoPhi` unified)

Primary φ–p / φ–d correlation function. Per \(k^*\) bin, fit the full \(M_{KK}\) spectrum after subtracting a scaled ROT or MIX template:

\[
S = F - \alpha B,\qquad
\alpha = \frac{\int_{M_\alpha} F\,dM}{\int_{M_\alpha} B\,dM},\qquad
C_{\mathrm{raw}}(k^*) = \frac{Y_{\mathrm{SE}}(k^*)}{Y_{\mathrm{ME}}(k^*)}.
\]

\(Y\) is the gaus-only integral of \(S\) in the channel `signalMin`–`signalMax` window. \(C_{\mathrm{norm}}\) scales \(C_{\mathrm{raw}}\) to mean 1 in `normQMin`–`normQMax`.

- **Default background template is ROT** (`kstarMassFitCfTemplate: rot`). MIX is a cross-check (`kstarMassFitCfCrossCheck: true`) because older MIX ROOT files still use a superseded mixing/cap scheme.
- Maker fills **wide** TH3 `hPhiMKK_vs_KstarSE/ME_<base>_wide` (full \(M_{KK}\), no signal-window cut at fill). ROT/MIX templates use `hPhiMKK_vs_Kstar{SE,ME}_phi_{rot|mix}_{proton,deuteron}_wide`. Histogram names are unchanged.
- YAML (`kstarMassFitCf*`): `Enabled`, `Template` (`rot|mix`), `CrossCheck`, `FitMassMin/Max`, `KstarBinWidth` (0.050 GeV/\(c\) in `auau3p85fxt_anaFemtoPhi`), `LowKstarMergeBins` (2 → first point covers \(0<k^*<0.1\) GeV/\(c\)), `AlphaMassMin/Max` (1.04–1.06; must not overlap `phi_proton_signal`), `WriteSidecar`.
- \(\alpha\) window must not overlap the signal window (Validate error). Overlap with the fit range is a warning.
- Invalid \(k^*\) bins are recorded (`kmf_fitstatus_*`): 0=ok, 1=fit fail, 2=negative yield, 3=low stat, 4=norm fail, 5=non-finite. Failed bins are omitted from \(C_{\mathrm{raw}}\) (no fake zero yield). Negative \(S\) bins are **not** clipped.
- QA PDF: `share/figure/<anaName>/<anaName>_checkHistAnaFemtoPhi_kstarMassFitCf[_jobid].pdf`. Sidecar: `..._CFkmf[_jobid].root`.
- Graph names: `CF_kmf_{rot|mix}_{base}_{slice}_{raw|norm}`, `kmf_Y_{SE|ME}_{rot|mix}_{base}_{slice}`, `kmf_fitstatus_{rot|mix}_{base}_{slice}`.
- Simple SE/ME count ratio in the main QA PDF is diagnostic only.
- `legacyCfPagesEnabled: false` (default) skips Topic 3, old direct mass-fit, and Method 5 pages.
- Old names (one-release YAML aliases only): Method3 / Method3AllKstar / `cfDirectPurityMode: method3` / `method3BkgSub*` / `purityDirectFitMass*` / `CF_method3_bkgsub_*` / PDF `..._Method3AllKstar` / sidecar `..._CFmethod3`.
- Closure toy: `tools/test_cf_kstarmassfit.C`.
- Hist keys generated by `script/generate_hist_anaFemtoPhi.py` (`WIDE_FEMTO_CHANNELS`).

### Topic / method 5: sideband CF-subtraction (`anaFemtoPhi` unified)

- **Distinct from kstarMassFitCF and Topic 3 `C_genuine`.** Off unless `legacyCfPagesEnabled`. Method 5 uses sideband correlation functions, not ME-mass `C_bkg`.
- Formula:
  - `CF_sig = norm(SE_sig / ME_sig)`, `CF_SB = norm(SE_SB / ME_SB)` (SBLR = left+right SE/ME sum by default; no width α).
  - `CF_CFsub = [CF_sig - (1-P) CF_SB] / P` with constant per-slice purity `P` from φ M_KK gaus+const fit (`fit_slice`) or YAML `cfSubPurityFixed`.
- Cache keys (non-destructive): `CF_CFsub_SBLR_<base>`, `CF_CFsub_SBL_*`, `CF_CFsub_SBR_*`, `CF_sig_method5_*`, `CF_SB_*`.
- Existing products kept: raw CF, count-level `CF_sig_sub_SBL|SBR|SBLR`, Topic 3 `CF_genuine`.
- YAML: `cfSubtractionMode` (`none|method5`), `cfSubPurityMode` (`fixed|fit_slice`), `cfSubPurityFixed`, `cfSubSidebandCombine` (`sumLR|avgCF_LR`), `cfSubWriteSidecarRoot`, `cfSubLowStatsRebinExtra` (extra rebin for t/³He/⁴He).
- Sidecar ROOT: `share/figure/<anaName>/<anaName>_checkHistAnaFemtoPhi_CFsub_<jobid>.root` (graphs + purity parameters + meta).
- Unit / toy tests: `tools/test_cfsub_method5.C` (`P=1`, `CF_SB=1`, hand numeric).
- Reference job for development: `7BB546C8496FB964E74D00B21D3B1505`.

### h–K± two-body CFs and Kubo-rule background (`anaFemtoPhi` unified)

- **Additive; all existing paths unchanged.** Everything below is behind off-by-default flags.
- **New track species** `phikaon_plus` / `phikaon_minus` (`particleKey: phi_kaon_plus|phi_kaon_minus`). They use **production** φ-daughter PID (`PassPhiDaughterTofPid`, same as real/ROT/MIX production K), NOT the loose TPC collection and NOT the anaFemtoKaon `kaon_minus` bachelor cuts. Registered via `speciesKeys` in the maker YAML. Pool copies store `tofMatch`, `mass2`, and `deltaOneOverBeta` so MIX can apply the same daughter PID using each track's origin-event TOF.
- **Two-body h–K± channels** `phikaon_plus_<h>` / `phikaon_minus_<h>` for `h ∈ {proton, deuteron, triton, he3, he4}` (partA = kaon species, partB = hadron). Filled by the existing `FillSameEventPairs`/`FillMixedEventPairs` (a same-event same-track guard was added to `TracksOverlap`). Hist keys `hKstarSE/ME[VsCent]_<channel>`, empty `hCF_<channel>` shell; `*Kstar` for p,d and coarser `*KstarCoarse` (150 bins) for t/³He/⁴He. Gated by `enableHKaonTwoBody`; macro pages gated by the same flag (`isHKaonTwoBodyEnabled()`).
- **Kubo-rule 3-body background** (`FillKuboTripletBackground`, gated by `enableKuboTriplet`, bases `phi_proton`, `phi_deuteron`):
  - Legacy 1D (signal-window gated): `hKstarTripSEKp/SEKm/Mix_<base>`, `hMKKtriplet_<base>`.
  - Full-mass TH3 when `kuboStoreFullMass: true`: `hKuboMKK_vs_KstarSEK{p,m}/Mix/KK_<base>` (x=M_KK, y=k*, z=cent9). KK topology = ME hadron + SE K⁺K⁻.
  - Reject counter: `hKuboNRejectShared_<base>`.
- **ROT full-mass TH3**: `hPhiMKK_vs_Kstar{SE,ME}_phi_rot_{proton,deuteron}_wide` (fill before mass window; channels `phi_rot_proton`, `phi_rot_deuteron`).
  - MIX full-mass TH3 (standard event-mixed KK: current K×buffer opposite K, psn0585-style): `hPhiMKK_vs_Kstar{SE,ME}_phi_mix_{proton,deuteron}_wide` (channels `phi_mix_proton`, `phi_mix_deuteron`; YAML `fullyMixedEnabled`). kstarMassFitCF uses these as the MIX background template (cross-check; default is ROT).
  - Old vs Kubo background / genuine CF (checkHist): from 1D triplets; legacy direct-mass-fit × Kubo closure pages when `legacyCfPagesEnabled` and Kubo are both on.
- **YAML flags**: `enableHKaonTwoBody`, `enableKuboTriplet`, `enableKuboGenuine`, `kuboStoreFullMass` (default false).

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

### Phi-near-track PID QA (`anaFemtoPhi` unified)

- **Not used in CF** — inclusive PID QA only (`FillPhiNearTrackPidQa`).
- Sample: all `PassTrackCuts` tracks (not bachelor-PID selected), excluding φ daughter indices.
- Selection: reconstructed φ in `m_eventCandidates["phi"]` with M_KK in `phiNearTrackSignalChannel` signal window; PRF `k*` vs track (mass hyp `phiNearTrackMassHyp`, default pion) below YAML thresholds.
- Histograms:
  - `hDedxVsP_PhiSignalNear_k1` / `hMass2ChargeVsP_PhiSignalNear_k1` — `k* < phiNearTrackMaxKstarLoose` (default 1.0 GeV/c)
  - `hDedxVsP_PhiSignalNear_k03` / `hMass2ChargeVsP_PhiSignalNear_k03` — `k* < phiNearTrackMaxKstarTight` (default 0.3 GeV/c)
- `m²×charge` y-axis covers ⁴He (`Mass2ChargeY`, ±30). TOF-matched tracks only for m² plots.
- Maker YAML: `phiNearTrackQaEnabled`, `phiNearTrackSignalChannel`, `phiNearTrackMaxKstarLoose/Tight`, `phiNearTrackMassHyp`.
- checkHist: one page (2×2) immediately before legacy CF pages (Page 15+).

### Phi-daughter production PID (`anaFemtoPhi` unified)

- **Loose K** (`kaonsPlus` / `kaonsMinus`): TPC quality + nσ (`PassKaonCuts`) plus collection filter `IsKaon` → `PassTofKaonPid` (`tofFallbackMode: tpcOnly` allows unmatched tracks). Used only for loose QA (`m_phiQaLoose`, `m_phiQaPreMassLoose`). `hNKaonPlus` / `hNKaonMinus` count this sample.
- **Production K**: `PassPhiDaughterTofPid` (full momentum \(p\), YAML `pMomKaonPID`). Same predicate for real φ, ROT, MIX, and `phikaon_*`. **Charge-asymmetric by default** (`phiDaughterKaonMinusRequireTof: true`):
  - K⁺: \(p \le pMomKaonPID\), no TOF allowed (TPC already applied); high \(p\) requires TOF
  - K⁻: **TOF match always required** (including \(p \le pMomKaonPID\)). Low-\(p\) TPC-only is K⁺ only
  - \(p > pMomKaonPID\), no TOF: rejected for both charges (`tpcOnly` must not pass high-\(p\) production K)
  - TOF-matched: `tofUseMass2Cut` / `tofUseDeltaInvBetaCut` as configured
- Do **not** make K⁺/K⁻ TOF policy charge-symmetric at FXT: low-\(p\) unmatched K⁻ is dominated by \(\pi^-\)/\(e^-\) and spoils \(\phi\) S/N and ROT \(M_{KK}\) templates.
- **Where production PID is required:** real φ candidates after `PassPairTofCut`, ROT, MIX (current and buffer daughters using stored TOF), and `phikaon_plus` / `phikaon_minus`.
- MIX does not apply a common DCA (cross-event). Pair topology and mixing sampler are unchanged.
- QA: `hPhiDauPid_*` (loose / prod / reject, K⁺/K⁻, category vs \(p\)/TOF) and `hPhiDauPidUsed_*_{real,rot,mix}_*` for daughters that entered production pairs. Generated by `script/generate_hist_anaFemtoPhi.py`.

## Event mixing (ME statistics)

- Config: `config/cuts/mixing/mixing_<ana>.yaml` via mainconf `mixing:` key (`MixingConfig`).
- Both modes use one eligible candidate-pair population: current A × buffered B plus,
  when `mixBothDirections` is true, buffered A × current B. The two directions are
  independent, so a missing current B does not suppress forward mixing and a missing
  current A does not suppress reverse mixing. Empty buffer-side candidate collections
  never enter the eligible population.
- `mixingMode`:
  - `randomSample` (default): uniformly sample without replacement from the eligible
    population, up to `maxMixedPairsPerEvent`.
  - `bufferAll`: exhaust the same eligible population (Zhangwei-like) and ignore
    `maxMixedPairsPerEvent`.
- P0-2 intentionally corrects the old `randomSample` behavior. The old implementation
  required both current A and B, derived its attempt count from the same-event
  multiplicities, sampled buffer events uniformly, and discarded draws with empty
  buffered B; it is no longer available as a physics mode.
- `maxMixedPairsPerEvent`: maximum attempted pairs per current event and channel for
  `randomSample` (default 500; 0 = unlimited).
- Weighting: every candidate-pair instance has equal inclusion probability in
  `randomSample`; `bufferAll` includes each once. A buffer event therefore contributes
  in proportion to its candidate-pair multiplicity, rather than receiving equal event
  weight. No additional multiplicity or event weight is applied. With a finite cap, a
  current event contributes `min(N_eligible, maxMixedPairsPerEvent)` attempts. Later CF
  normalization can absorb total ME scale but not a multiplicity-correlated shape change.
- QA: `hMixSamplerQA` is a channel-indexed counter matrix. Its status bins verify
  `eligible = attempted + skipped_by_cap`, `attempted = filled + skipped_by_pair_cut`,
  and `skipped = eligible - filled`. `selected_empty_buffer_direction` must remain zero;
  forward/reverse eligible and filled counts diagnose the two directions separately.
  These counters are raw counts; centrality/event weights are not applied.
- **Mixing bin:** `GetMixingBin(vz, cent9, psi2)` uses Vz bin × **cent9 as bin index** × EP bin. ME pairs only **different events** in the same bin (never same-event pairs; those are SE).
- **Shared-track provenance:** each resonance daughter is stored as `(eventIndex, trackIndex)`. Real and rotated φ
  daughters share the current event index; fully mixed φ daughters retain their distinct current/pool event indices.
  Overlap rejection requires both fields to match, so a reused local track index from another event is not rejected.
- **Centrality slices in checkHist:** `pct_0_10` etc. project cent9 ranges **after** fill; Maker mixing stays at cent9 resolution.
- Changing mixing requires a **new batch run**; cent-slice CF projection uses existing `hKstar*VsCent_*` in merge ROOT.
- **Rollout:** compare `randomSample` and `bufferAll` on a short joblist before full catalog; check high-k* `C→1`, ME shape, job time, merge ROOT size.
- Production mainconfs keep the `randomSample` mode name and receive its corrected semantics.

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

YAML keys on femto config (not `PIDCutConfig`): `protonChargeMode`, `protonMaxDca`, `protonMinPtPre`, `protonMinPtPair`, `protonMaxPtPair`, `protonMaxAbsEta`, `protonMaxAbsNSigma`, `protonMinNHitsFit`, `protonMinNHitsRatio`, `protonTofMomentumThreshold`, `protonMinMass2`, `protonMaxMass2`, `protonMinRapidityCm`, `protonMaxRapidityCm`.

TOF/m² gate is **only** in `PassTofProtonPid` (also used by `IsProton` and `PassFemtoProtonCuts` — same predicate, no second rule):

- \(|p| <\) `protonTofMomentumThreshold`: TPC-only (no TOF match required; **no m² veto** even if TOF-matched).
- \(|p| \ge\) threshold: TOF match required and `protonMinMass2` ≤ m² ≤ `protonMaxMass2`.

Does **not** use global `PIDCutConfig.requireTOF` / `tofFallbackMode` (those remain for φ-daughter / collection paths).

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

## Fully-mixed KK (`phi_mix`)

- **Species:** `phi_mix` / `particleKey: phi_fully_mixed` when `fullyMixedEnabled: true`.
- **Population:** production-PID daughters (`PassPhiDaughterTofPid`) in both directions combined — current K⁺ × buffered K⁻ and buffered K⁺ × current K⁻ — into one pair-index space. Eligible pairs also pass invMass>0, opening-angle, and pair-rapidity cuts.
- **Cap:** `fullyMixedMaxCandidates` is the stored-candidate count **per current event** on the combined set (not per direction). Production uses `2000`. `<=0` keeps every eligible pair (validation only; do not run uncapped on full production).
- **Sampling:** lazy Fisher–Yates permutation of 64-bit flat pair indices (`include/FemtoPhiMixSampler.h`). The first `cap` eligible pairs in that random order are stored, which is a uniform sample without replacement from the eligible set. No sampling weights / event reweighting.
- **RNG:** independent SplitMix64, **not** `gRandom`. YAML `fullyMixedSamplingSeed` (production `314159`). `>0` is fully reproducible for the same input; `0` is time-based and `Validate()` warns. Changing `rotationN` does not change the `phi_mix` sample.
- **QA:** `NPairPopulation` is the exact post-PID combination count. `NAttempted` is how many indices were evaluated. `EligibleLowerBound` is a lower bound when the scan stops at cap+1; `NEligibleExact` is filled only on a full scan. `hPhiMix_MKK` is stored candidates only (not a raw/unscanned MKK).
- **Tests:** `make test-phi-mix-sampler` (standalone). Does not change `FillMixedEventPairs`.

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
- **Variant `auau3p85fxt_anaFemtoPhi_dedxOnly`:** nuclei femto TOF m² off (`deuteron/triton/he3/he4TofMomentumThreshold: 99`, NuclearId `m2_selection: false`). **Proton:** `protonTofMomentumThreshold: 2.0` — low \(|p|\) TPC-only, high \(|p|\) TPC+TOF(m²). **Deuteron:** dE/dx-only. Mainconf: `main_auau3p85fxt_anaFemtoPhi_dedxOnly.yaml` (same macros/hist).
- **Species keys:** `proton`, `deuteron`, `triton`, `he3`, `he4`, `phi`, `phi_rot` (rotation for proton only).
- **Channels (21):** `phi_<bachelor>`, `_signal`, `_leftSB`, `_rightSB` per bachelor; plus `phi_rot_proton`.
- **Nuclear ID:** `StNuclearIdHelper::IsTriton()` / `IsHe3()` with `requireBestSpecies: true` (bestType 1=triton, 2=he3).
- **QA hist prefixes:** `hP_`, `hDeuteron_`, `hTriton_`, `hHe3_`, `hHe4_`; k* keys `hKstarSE/ME_<channel>`.
- **checkHist:** `checkHistAnaFemtoPhi.C` — bachelor-loop QA; single PDF (no CF-only PDF).

