# StMaker/common

Shared compiled helpers built into `lib/libStCommon.so` and loaded by every `run_ana*.C` before Maker libraries.

## Build / load

- Sources: `StMaker/common/*.cxx` (auto-collected by the Makefile).
- Output: `lib/libStCommon.so` (links against `libStarAnaConfig` and `libStRefMultCorr`).
- Each `libSt*Maker.so` links `-lStCommon`; runners must `gSystem->Load(".../lib/libStCommon.so")` and include `-lStCommon` in `AddLinkedLibs`.

## Phi KK reconstruction

**Source of truth:** [`StPhiKKReconstruction.h`](StPhiKKReconstruction.h) / [`StPhiKKReconstruction.cxx`](StPhiKKReconstruction.cxx)

Helix DCA, φ→K⁺K⁻ invariant mass, opening angle, pair rapidity, **unified production TOF PID** (`PassPhiDaughterTofPid` / `PassPairTofCut`; same function for K⁺ and K⁻), and TOF fill logic used by `StPhiMaker` and `StFemtoMaker` live here. Cut thresholds come from YAML via `ConfigManager` (`PhiCutConfig`, `PIDCutConfig`). Predicate without STAR deps: [`include/PhiDaughterPid.h`](../../include/PhiDaughterPid.h). `PassTofKaonPid` is the loose collection/fallback filter only (`tofFallbackMode`); it is not production pair PID.

Makers keep event loops, histogram filling, and analysis-specific orchestration (e.g. femto `BuildResonanceCandidates`, SE/ME pairing).

## Nuclear ID helper

**Source of truth:** [`NuclearIdDeDxVsMom.h`](NuclearIdDeDxVsMom.h), [`StNuclearIdHelper.h`](StNuclearIdHelper.h) / [`StNuclearIdHelper.cxx`](StNuclearIdHelper.cxx)

LogPoly dE/dx vs p calibration tables (header-only constants) and light-nucleus identification (`IsHe4`, TPC nσ, TOF m², best-species). Operational cuts (`maxNSigmaNuclear`, `m2_selection`, etc.) come from `NuclearIdCutConfig` YAML. Used by `StFemtoMaker` for ⁴He bachelor candidates without loading `StNuclearIdMaker` on the chain.
