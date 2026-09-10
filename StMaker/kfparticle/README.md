# PicoDst → full KFParticle adapter

`StPicoKFParticleInterface` is a compiled, event-level adapter. The public header
contains only plain result/statistics types and a PIMPL pointer; no SIMD/core
implementation is exposed to ROOT 5 CINT or Maker dictionaries. The KF core is
`star_analyzer_kfp`-namespaced and linked only through the KF-specific libraries.

## Reference and correspondence (2026-09-07)

The reference behavior below is the default `selectionProfile: kf_reference`.
The explicit `lambda_imp5` comparison profile is documented separately below.

Primary reference:
`/star/u/xwu2/test/ampt_test_xing/KFTree_lambda/StRoot/StKFParticleAnalysisMaker/StKFParticleInterface.cxx`.
The core is a separate, coherent `.DEV2` snapshot documented in
`../../StRoot/KFParticle/PROVENANCE.md`; an old user's scalar/SIMD core is not mixed
with current core headers.

| Reference function | Adapter implementation |
| --- | --- |
| `GetTrack` | `KfParticleHelper::BuildTrack`, via SL24y `StDcaGeometry::GetXYZ` |
| `GetTofPID` | `Impl::Pid`, charge-specific fourth-order mean/width polynomials |
| `GetPID` | `Impl::Pid`, TPC π/K/p hypotheses intersected with valid TOF hypotheses |
| `AddTrackToParticleList` | `Impl::ProcessEvent`, per-hypothesis `KFParticle` and primary classification |
| `SetPrimaryProbCut` | compatibility initialization only with external PicoPV; PV refit is not called and Finder's threshold is set independently |
| `InitParticles` / `AddPV` / `ReconstructParticles` | Topo `Init`, Finder `Init`, external PV registration, `FillPVIndices`, `SortTracks`, `ReconstructParticles` |

Track conversion retains the reference's `NDF=1`, initial `chi2=0`, original
track ID and charge. It does not add the helix fit chi2 to the decay fit. The
SL24y covariance unpacking follows the conditional
`StPicoTrackCovMatrix::dcaGeometry()` implementation without globally defining
`__TFG__VERSION__`.

The `dedx_pull` profile reproduces the public-input equivalent of SL24y's
conditional `StPicoTrack::dEdxPull(mass, 1, 1)`: `gMom().Mag()/mass`,
`1e-6*dEdx()`, `dEdxError()`, and `StdEdxPull::Eval(..., fit=1, charge=1)`.
The distinct `pico_nsigma` profile uses the saved PicoDst values. These quantities
are not silently interchanged. Failed/nonfinite pulls are rejected. There is no
dataset-specific extra `Calibration()` correction: that call is commented out
in the primary xwu2 reference.

TOF coefficients are configuration data (`tofMean` / `tofSigma`, 30 elements
each), ordered π+, K+, p, π−, K−, anti-p, with ascending polynomial powers in
each row. They come from the reference `GetTofPID`; they are not asserted to be
universally calibrated for every dataset. Momentum clamping and PID thresholds
are also configured. Soft TOF accepts every matching hypothesis, while strict
TOF accepts only the closest one. Missing/invalid beta allows TPC-only hypotheses
in either mode, matching the reference's absence-of-measurement behavior.
Optional kaon cleaning can require kaon TOF in its configured momentum interval.

## Deliberate corrections / Lambda scope

- TOF index zero is valid. Referenced missing/out-of-range traits are fatal input
  errors; negative, zero and nonfinite beta are not interpreted as measurements.
- The current event's magnetic field is set **before** calculating track/PV
  deviations, avoiding any dependence on the preceding event's field.
- Track and covariance counts must match. Covariance finiteness and diagonal
  bounds, finite positive PV errors, valid IDs and daughter mappings are checked.
  Original track IDs are not used as vector offsets. Duplicate original IDs are
  rejected explicitly, and multiple hypotheses of one track retain one mapping.
- The π/K/p multi-hypothesis path is retained, including the reference's unknown
  PDG `-1` input when no hypothesis matches. Light-nucleus/helium PID, rotation,
  fixed-target PV position cuts, MuDst, MC and embedding output are not part of
  this PicoDst Λ adapter. Nuclei do not serve as Λ→pπ daughters; this is an explicit
  input/output scope, not a scalar replacement of the reconstruction engine.
- Quality/PID thresholds are YAML settings. The xwu2 reference currently has an
  **active** fit/max-hit ratio 0.52 and η in [−2.4, 0]; the AuAu19 profile is a
  documented dataset adaptation, not a claim of identical PID efficiency.
- PicoDst's supplied PV and covariance are registered without PV refitting.
  `interfaceChiPrimaryCut` compares the actual `GetDeviationFromVertex` return
  value. No extra square/square-root transformation or scalar helix-DCA cut is
  inserted. The compatibility `primaryProbCut` initialization is not an operative
  cut in this external-PicoPV path: the PV reconstructor is not run and Finder
  ChiPrimary2D is overwritten by the explicit `finderChiPrimary2D`. The cleaned
  AuAu13p5 YAML and effective dump omit that unused probability parameter.

## Opt-in Imp5 common cuts (2026-09-08)

`selectionProfile: lambda_imp5` preserves the full Finder/Topo reconstruction
engine while matching the active legacy Lambda cuts. Existing configurations
default to `kf_reference`; no legacy Maker or normal KF selection is changed.

- Require `pidProfile: pico_nsigma` and disable TOF, strict TOF, kaon cleaning
  and HFT-only selection. The saved p/pi pulls are tested independently with
  inclusive |nSigma| <= threshold. Only proton/pion hypotheses are supplied;
  nonmatching tracks retain the reference unknown-PDG input. With anti-Lambda
  disabled, only p+ and pi- hypotheses can become daughters.
- Hit count/ratio follow the legacy branch, including skipping the ratio if
  nHitsMax is zero. Unused pT, eta, nHitsDedx, dEdxError, kaon and TOF numeric
  thresholds have been removed from the Imp5 YAML and effective dump. Their
  internal fields remain for reference-profile/archived-input compatibility. Finite,
  nonzero momentum and covariance/track-identity safety checks remain.
- `imp5MinDCAProton` / `imp5MinDCAPion` apply to the original three-scalar
  PicoTrack `gDCA` overload used by StLambdaMaker, not to KF PV chi2 or a new
  closest-helix-point calculation. These independent species thresholds are
  explicit YAML data (0.7 / 1.0 cm in the AuAu13p5 profile).
- `imp5MaxPathLength` (100 cm) is checked using the original two
  StPhysicalHelixD trajectories **after Finder has found a candidate**.
  `KfParticleHelper::FillImp5PathLengths` mirrors the legacy helix constructor
  and `pathLengths` call, saves Double_t physical lengths and a validity flag,
  and never replaces the KF mass, momentum or vertex. KF transport dS is not a
  physical path length and is not used for this cut.
- The Maker applies the matching daughter-distance, parent-PV-distance and
  pointing thresholds to KF reconstructed quantities. Therefore their numeric
  limits match Imp5, but their per-candidate values need not equal Helix values.
  Existing covariance/PV/Finder/Topo requirements, including the upstream
  parent PV chi2/NDF < 3 selection, remain unchanged.

The output candidate tree additionally stores `protonDcaToPv`,
`pionDcaToPv`, `protonHelixPathLength`, `pionHelixPathLength` and
`imp5PathValid`. These are Imp5 diagnostics; in the reference profile the flag
is false and numeric defaults are not a DCA/path measurement. A raw Imp5
candidate may fail the path guard; only a selected candidate must pass it.

For this profile only, `KfEventSelection` explicitly selects
`legacy_lambda_imp5`: maxNTr plus the existing bad-run/centrality sequence,
without extra Vz/Vr/RefMult/VPD cuts unused by current StLambdaMaker. Mode and
`qaVertexByMode.<mode>.center` are validated for diagnostic QA; the dedicated
Imp5 event file has only these centers and `maxNTr`. Archived full
`vertexByMode` Imp5 inputs remain readable. Metadata lists the policy, center
and applied flags, without pretending that inactive vertex/RefMult/VPD bounds
are effective cuts. The usual mode-dependent KF
event selection is unchanged for `kf_reference`.

Configuration validation rejects unknown profiles, missing/invalid Imp5
thresholds, TOF/HFT combinations, and accidentally enabled Imp5 thresholds in
the reference profile. See `tests/check_kfparticle_imp5_output.C` for real-output
consistency QA and the main [workflow reference](../../docs/REFERENCE.md).
The three cleaned AuAu13p5 mainconfs reference no generic track/PID/V0/mixing
compatibility files. A `# config-generator: manual` marker prevents the generic
configuration generator from recreating them or overwriting custom macros.
Cleanup and non-regression evidence: [cleanup record](../../mdfiles/unused_lambda_cut_cleanup_20260908.md).
The local comparison record includes unchanged-default regression tests:
[Imp5 results](../../mdfiles/lambda_helix_kf_imp5_comparison_10000_20260908.md).

## Candidate semantics and upstream cuts

`Candidates()` contains finite, valid ±3122 results from Topo's `GetParticles()`
**after its upstream selection**, before the Maker's optional final cuts. This
is not the mass-constrained secondary-Lambda container. The returned parent
`mass`, `massError`, decay vertex, momentum and `chi2Ndf` have no parent PDG-mass
constraint. A distinct copy receives `SetProductionVertex(PV)` to calculate
`topoChi2Ndf`, decay length and its error. The raw vertex-line length/error and
its significance are stored separately from PV-constrained decay length.

The current `.DEV2` and xwu2 Topo `ReconstructParticles()` both call
`SelectParticleCandidates()`, but their historical thresholds differ: the adopted
current `.DEV2` requires PV-constrained chi2/NDF **strictly below 3**, while the
old xwu2 copy uses **below 5** (`KFParticleTopoReconstructor.cxx:635`). Failed
candidates become PDG −1. The current core's mass-distance competition loop is
disabled by `#if 0`. The adopted `.DEV2` behavior is preserved, so a looser Maker
topology cut cannot recover candidates already removed upstream. This is an
explicit core-version difference from the old user analysis, not a claimed
bit-for-bit reproduction of its selection.
Other internal Finder conditions are also retained, including STAR `__ROOT__`
branches. “Raw mass” does not mean “no upstream topology selection.”

Composite daughter IDs refer to the output particle array. A track-derived
daughter's single daughter ID is the original track ID, resolved through the
event-local ID→Pico-index map. PDG and charge identify p versus π, not assumed
daughter ordering. Daughter PID values in the result use the active profile;
TOF numbers are meaningful only when the corresponding `HasTof` flag is true.

Counters distinguish quality/covariance tracks, accepted/unknown PID hypotheses,
primary hypotheses, all Finder output entries (including input tracks and
logically removed candidates), surviving ±3122, invalid candidates and valid
results. A physical event with zero tracks is an empty valid event; an input
file with zero events is handled as an error by the analysis macro. Configuration
must remain unchanged for the adapter's lifetime; configure a new adapter when
changing cuts. Constructor and event entry validate configuration.

`BackendDescription()` records the core source fingerprint, defines,
namespace, reference source and active upstream Λ topology restriction. Full
build, real-event and reference-comparison evidence is recorded separately;
this source correspondence alone is not a physics-validation claim.

## ROOT 5 CINT const-initializer regression

The first real-event run exposed a ROOT 5/CINT issue: an interpreted runner
using `const Long_t result = gROOT->ProcessLine(...)` can propagate its const
initialization context into the nested STAR dEdx table macro. The ordinary
`row.nknots = 14` assignment then reports `Re-initialization ignored const nknots`.
SL24y's `StdEdxModel` subsequently dereferences a failed spline singleton and
crashes. This is not a change in the KF fit or a ROOT 6 requirement.

A minimal STAR+`StBichsel` probe, **without any project KF libraries**, isolated
the issue in `tests/probe_std_edx.C`:

- Mode 6 uses a const declaration initializer and reproduces the warning/crash,
  exit status **139**. **This diagnostic mode intentionally crashes.**
- Mode 7 uses a separate declaration and assignment and succeeds, exit status
  **0**, with the original 14-knot table, 52.0 eV/electron scale and
  `StdEdxPull::EvalPred(4., 1, 1) = 2.62464e-06`.
- Modes 0/1/4/5 succeed with/without the project core and with/without metadata
  lookup. Neither KF-library loading nor dictionary priming explains the failure.

Keep interpreter-reentering calls outside const declaration initializers in
the ROOT 5 runners, including the call into the compiled test module:

```cpp
Long_t result = 0;
result = gROOT->ProcessLine(call.Data(), &error);
```

The runner/test-runner fix does not modify the STAR model, calibration table,
PID formula or profile, and does not add a table-loading bridge or a silent PID
fallback. It does not prohibit ordinary const variables in compiled C++.
The unchanged SL24y table `StarDb/dEdxModel/spline3LndNdxL10.C` used by the probes
has SHA-256
`ed99b0afb0161c5a1825deaebdc7f38d3e08ee884497c73fad4cf1e33b6d6694`.
