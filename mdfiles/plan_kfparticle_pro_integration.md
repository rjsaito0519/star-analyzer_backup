# Plan: KFParticle on `pro` / `SL24y` (RefMultCorr-style)

Handoff for whoever implements Kalman-filter Λ / Ξ reconstruction in this repo. Motivation: replace or supplement helix V0 (`StLambdaMaker`) with KF topology fits, especially for **Λ (PDG 3122)** and **Ξ (PDG 3312)**.

Related principles: [`PHILOSOPHY.md`](../PHILOSOPHY.md), [`docs/ai/skills/reuse-star-stroot.md`](ai/skills/reuse-star-stroot.md), [`StRoot/StRefMultCorr/`](../StRoot/StRefMultCorr/).

---

## Situation (as of 2026-09-02)

- Official STAR **`pro` / `SL24y` `$STAR/lib` does not ship KFParticle**. The package lives in **`.DEV2`** (`cons` + often `starver .DEV2` then TFG). Published analyses copy the tree into their own `StRoot/` and build with `cons`.
- This framework already vendors **`StRefMultCorr`** the same way: source in `StRoot/`, `make` → `lib/libStRefMultCorr.so`, `run_ana*.C` loads **`$PWD/lib/`** (never `$STAR/lib` of the same name). Pico I/O stays on the analysis `libraryTag` (typically **`SL24y`** from `analysis_info`; login shells may still show `starver pro`).
- **We will not switch job runtime to `.DEV2`.** Mixing `setupDEV2.csh` / `cons` / `bfc(...,kfpAna)` with `script/setup.sh` would fight `libraryTag` and Pico libraries.
- **PicoDst `TrackCovMatrix` is filled** on the 3.85 GeV FXT P24iy files we could open locally (`tmp/pico/bench1.picoDst.root` run `22121037`, `bench_heavy.picoDst.root` run `22172001`): `nTrack == nCov` per event, `isBadCovMatrix() == false` for all scanned tracks. Catalog/`xrootd` files were not readable from the AL9 login node; batch workers still use P24iy of the same production.
- **`StPicoTrackCovMatrix::dcaGeometry()` is compiled only under `__TFG__VERSION__`.** Do **not** define that macro globally. Unpack `params` / `sigmas` / `correlations` into `StDcaGeometry::set()` in **our** helper (TFG implementation is ~15 lines; see references).
- Existing **`StLambdaMaker`** is helix + DCA pairing. Keep it; KF is a separate common helper that Makers call.

---

## Goals / non-goals

**Do**

- Vendor KF as a library, same pattern as RefMultCorr.
- Keep reconstruction **modular** in `StMaker/common/` (like `CentralityHelper`).
- Put **all** KF cuts, PDG list, and track preselection in **YAML** (no magic numbers in C++).
- Stay on current `libraryTag` (`SL24y` / `pro`-class Pico).
- Enable `picoMaker->SetStatus("TrackCovMatrix", 1)` in analysis macros that use KF.

**Do not**

- Copy `StKFParticleAnalysisMaker.cxx` wholesale (flow, TMVA, `EventClass`, EP recenter).
- Use `lMuDst.C` / `bfc` / `source setDEV2.csh` as the production event loop.
- Hardcode χ², L/dL, χ²_topo, nHits, nσ, mass windows, or PDG lists in Maker/helper.
- Load KF from both `$PWD/lib/` and `$STAR/lib/`.
- Vendor `StFlowFunc` / paper-only tree classes into `StRoot/`.

---

## Architecture (RefMultCorr analogue)

| RefMultCorr today | KFParticle target |
|-------------------|-------------------|
| `StRoot/StRefMultCorr/` + `PROVENANCE.md` | `StRoot/KFParticle/` (+ optional `KFParticlePerformance/` later) + `PROVENANCE.md` |
| `lib/libStRefMultCorr.so` | `lib/libKFParticle.so` |
| `StMaker/common/CentralityHelper` | `StMaker/common/` KF wrapper (e.g. `KfParticleHelper`) |
| `config/cuts/centrality/*.yaml` | `config/cuts/kf/` (or equivalent) referenced from mainconf |
| `run_ana*.C` loads RMC **before** Maker | Load `libKFParticle.so` **before** `libStCommon.so` / Maker |

```
StChain
  StPicoDstMaker          // $STAR Pico; Track + TrackCovMatrix ON
  StXxxMaker              // analysis: hist / femto / pairing
       |
       +-- KfParticleHelper (StMaker/common)   // YAML in, KFParticle candidates out
              |
              +-- libKFParticle.so (vendored Finder / KFParticle)
              +-- Pico cov → KFPTrack unpack (no __TFG__VERSION__)
```

**Placement rule:** `StRoot/` = third-party math + Finder only. **No analysis cuts, no histograms.** Physics consumers stay in `StMaker/St*Maker/`. Shared reconstruction stays in `StMaker/common/` → `libStCommon.so`.

---

## Implementation method

Follow [`docs/ai/skills/reuse-star-stroot.md`](ai/skills/reuse-star-stroot.md) Part C.

### 1. Vendor source

Preferred copy source (canonical STAR, not paper-local patches):

- `/star/nfs4/AFS/star/packages/.DEV2/StRoot/KFParticle/`
- `/star/nfs4/AFS/star/packages/.DEV2/StRoot/KFParticlePerformance/` (MC QA only; skip until needed)

Working PicoDst copies (already used for Λ/Ξ trees; include STAR `StKFParticleInterface`):

- `/star/u/oura/gpfs/papers/psn0878/AnaPico/Xi/KFtree_xi/StRoot/`
- `/star/u/oura/gpfs/papers/psn0878/AnaPico/Lambda/KFtree_lam/StRoot/`

**Copy into this repo**

- `KFParticle/*.cxx,*.h` (Finder + SIMD + TopoReconstructor).
- **Do not** copy `StKFParticleAnalysisMaker.cxx` as-is.
- Take **`StKFParticleInterface.{h,cxx}`** as a reference, then:
  - delete `StFlowFunc` / EP flags / MuDst-only paths if unused;
  - replace `cov->dcaGeometry()` with a local unpack;
  - expose `ProcessEvent(StPicoDst*)`, `GetParticles()`, `AddDecayToReconstructionList`, cut setters.

Record copy date, absolute source path, and diffs in `StRoot/KFParticle/PROVENANCE.md`.

### 2. Build (`Makefile`)

KF is heavier than RefMultCorr:

- Compile flags: `-DKFPARTICLE`, include `-IStRoot/KFParticle`, **homogeneous field** (`HomogeneousField` / `__ROOT__` as in upstream `KFParticleDef.h`).
- **Vc** is required for `KFParticleFinder` / `KFParticleSIMD` (`#include <Vc/Vc>`). STAR ships Vc at `/star/nfs4/AFS/star/packages/repository/StRoot/Vc`; cvmfs also has `vc-0.7.4` on the SL7 ROOT stack. Link it; do not assume `$STAR/lib` under `SL24y`.
- You will likely need **`-lStEvent`** for `StDcaGeometry` (not in current Maker `STAR_LDFLAGS`).
- Build with **`./script/singularity_make.sh <mainconf>`** (same SL7 / `sl73_*` as farm). Host AL9 `make` is not the farm-bound path.

If Vc/SIMD blocks the first Makefile iteration, a **scalar-only** subset (`KFParticle` + `KFParticleBase` + `KFPTrack` + `KFVertex`, pairing in the helper) is a fallback for Λ only. **Ξ cascade should use Finder**, which is what the papers run.

### 3. Common helper (this is the product API)

Suggested class in `StMaker/common/` (name can vary):

- `Init(const KfCutConfig&)` — PDG reconstruction list, Finder cuts, track QA cuts from YAML.
- `Process(StPicoDst*)` — PV → `KFVertex`; tracks → `KFPTrack`; `ReconstructParticles()`.
- Accessors: particles with PDG 3122 / −3122 / 3312 / −3312; daughter ids; mass, pT, L, L/dL, χ², χ²_topo, nσ of daughters.

Makers **must not** reimplement Finder cuts. They only apply analysis-level selection that is also YAML (e.g. mass window for a femto pair) if still needed after KF.

### 4. YAML (no hardcoded parameters)

Add a dedicated cut YAML (one concern per file). Examples of keys that papers currently bake into macros — **all of these belong in YAML**, with paper values as **defaults in the YAML file**, not in `.cxx`:

- Reconstruction list: `3122, -3122, 3312, -3312`
- Track: `nHitsFit`, `nHitsDedx`, `dEdxError` range, `pt`/`eta` windows
- Finder 2-body: `chiPrimary`, `chiPrimary2D`, `chi2_2D`, `ldl_2D`, `maxDistance`, `LCut`
- Ξ/Ω: `ldlXiOmega`, `chi2TopoXiOmega`, `chi2XiOmega`
- PID mode flags: soft TOF / soft kaon (booleans)

Wire it from `config/mainconf/main_<anaName>.yaml` like `centrality:`. Extend `ConfigManager` / `include/cuts/` the same way as `CentralityCutConfig`.

Paper-side numbers (for YAML comments / first draft only), from  
`/star/u/oura/gpfs/papers/psn0878/AnaPico/Xi/KFtree_xi/StRoot/macro/analysispicodst_ldxi_pol.C`:

- `SetChiPrimaryCut(3)`, `SetMaxDistanceBetweenParticlesCut(1.5)`, `SetLCut(1.0)`
- `SetChiPrimaryCut2D(3)`, `SetChi2Cut2D(10)`, `SetLdLCut2D(3)`
- `SetLdLCutXiOmega(3)`, `SetChi2TopoCutXiOmega(10)`, `SetChi2CutXiOmega(10)`

### 5. Chain / load order

`analysis/run_anaXxx.C`:

1. STAR Pico libs  
2. `libStarAnaConfig.so`  
3. `libStRefMultCorr.so`  
4. **`libKFParticle.so`**  
5. `libStCommon.so`  
6. Maker `.so`  
7. `AddLinkedLibs` must include `-lKFParticle`

`analysis/anaXxx.C`: keep `StChain` → `Init` / `Make(i)` / `Finish`. No custom event loop. `SetStatus("TrackCovMatrix", 1)`.

### 6. Pico cov unpack (required on `pro`)

TFG (`/star/nfs4/AFS/star/packages/.DEV2/StRoot/StPicoEvent/StPicoTrackCovMatrix.cxx`):

```cpp
// errMatrix[15] from mSigma × mCorr, then StDcaGeometry::set(params(), errMatrix);
```

`StDcaGeometry::GetXYZ(xyzp, CovXyzp)` then `KFPTrack::SetParameters` / `SetCovarianceMatrix` is already in paper `StKFParticleInterface::GetTrack`.

Skip `isBadCovMatrix()` tracks.

---

## Suggested work order

1. YAML schema + empty helper stubs (so Makers can compile against the API).
2. Vendor `KFParticle` + Makefile + `PROVENANCE.md`; `singularity_make.sh` until `libKFParticle.so` links.
3. Cov unpack + `Process(StPicoDst*)` + Λ PDG only; mass QA hist via existing `HistManager`.
4. Enable Ξ in YAML reconstruction list; daughter / granddaughter accessors.
5. Hook one analysis (Λ or femto) through the helper; do not fork a second KF copy in that Maker.

---

## Reference code (read these first)

### Official STAR (`.DEV2`, not `pro`)

```
/star/nfs4/AFS/star/packages/.DEV2/StRoot/KFParticle/
/star/nfs4/AFS/star/packages/.DEV2/StRoot/KFParticlePerformance/
/star/nfs4/AFS/star/packages/.DEV2/StRoot/StKFParticleAnalysisMaker/
```

`SL24y` / `pro` `StRoot` have **no** `KFParticle` directory. Pico **does** have `StPicoTrackCovMatrix` and `StPicoDst::trackCovMatrix()`.

### Published analyses (`/star/u/oura/gpfs/papers`)

| What | Path |
|------|------|
| Ξ KF tree (best walkthrough: README + macro + Interface) | `/star/u/oura/gpfs/papers/psn0878/AnaPico/Xi/KFtree_xi/` |
| Λ KF tree | `/star/u/oura/gpfs/papers/psn0878/AnaPico/Lambda/KFtree_lam/` |
| K0S | `/star/u/oura/gpfs/papers/psn0878/AnaPico/K0S/KFtree_k0s/` |
| Ω | `/star/u/oura/gpfs/papers/psn0878/AnaPico/Omega/KFtree_omega/` |
| p–Λ femtoscopy KF tree | `/star/u/oura/gpfs/papers/psn0861/pL/papercode/Step2_CF_from_Data/Step2_1_KFParticle_Tree/` |
| p–Ξ | `/star/u/oura/gpfs/papers/psn0861/pXi/1_kfptree/` |
| FXT ~3.9 GeV mini-tree | `/star/u/oura/gpfs/papers/psn0864/FXT_3p9GeV/1_produceMiniTree/` |

Paper **runtime** (do not copy into this repo’s jobs):

- `/star/u/oura/gpfs/papers/psn0878/AnaPico/Xi/KFtree_xi/setDEV2.csh` (Λ/K0S/Ω trees have the same file) — `starver .DEV2`; `source $STAR/setupDEV2.csh`; then a TFG tag (`TFG23e` in psn0878 Xi).
- `cons` then `root4star ... analysispicodst_ldxi_pol.C`
- Chain string includes **`kfpAna`**: `lMuDst(..., "ry2016,RpicoDst,mysql,kfpAna,quiet,nodefault", ...)`
- Decay list after `chain->Init()`: `StKFParticleInterface::instance()->AddDecayToReconstructionList(3122)` etc.

Core call sequence (Pico): `BookVertexPlots` → `ProcessEvent(picoDst)` → loop `GetParticles()` / `GetPDG()`.

### STAR users (same package, extra copies)

- `/star/u/gwang1/kfp/` — QA / embedding for KFParticle (psn0878 authors).
- `/star/u/xwu2/test/ampt_test_xing/KFTree_lambda`, `.../KFTree_ks0`
- Yuri Fisyak / Maksym Zyzak: upstream in `.DEV2` (`StKFParticleAnalysisMaker` author tag).

### This repo

- Vendoring template: `StRoot/StRefMultCorr/{PROVENANCE,README}.md`, `Makefile` `RMC_DIR`, `analysis/run_anaPhi.C` Load order.
- Helper template: `StMaker/common/CentralityHelper.{h,cxx}`
- Helix Λ (not KF): `StMaker/StLambdaMaker/`
- Pico lists: `config/picoDstList/auau3p85GeVfxt.list`, `auau19GeV.list`
- Local filled Picos used for cov check: `tmp/pico/bench1.picoDst.root`, `tmp/pico/bench_heavy.picoDst.root`

---

## Pico / chain checklist

- [ ] `TrackCovMatrix` status ON.
- [ ] `numberOfTrackCovMatrices() == numberOfTracks()` (or skip bad matrices).
- [ ] `bField` from `StPicoEvent` → `KFParticle::SetField` / Interface `SetField`.
- [ ] Global tracks for V0 daughters (do not require primary-only before KF).
- [ ] Farm build: `singularity_make.sh` with the analysis mainconf.

---

## Out of scope for the first merge

- Paper `setDEV2.csh` + `cons` farm XML as a second production path.
- TMVA charm ntuple path in `StKFParticleAnalysisMaker`.
- Event-plane subtraction flags inside the KF interface (`StFlowFunc`).
- Replacing `StLambdaMaker` until KF mass/yield QA exists.
