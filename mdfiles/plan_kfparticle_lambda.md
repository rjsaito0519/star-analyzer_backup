# Implementation plan: Lambda reconstruction with KFParticle

## Purpose

Add an independent analysis path that reconstructs
\(\Lambda \rightarrow p + \pi^-\) with KFParticle while preserving the
current helix-based analysis.

New entry points:

- `analysis/anaLambda_KFParticle.C`
- `analysis/run_anaLambda_KFParticle.C`

The implementation uses the current `StLambdaMaker` as a reference, but
creates a separate `StLambdaKFParticleMaker` rather than replacing it.

References:

- [`../PHILOSOPHY.md`](../PHILOSOPHY.md)
- [`../docs/ai/AGENT_RULES.md`](../docs/ai/AGENT_RULES.md)
- [`../docs/ai/skills/add-new-analysis.md`](../docs/ai/skills/add-new-analysis.md)
- [`../docs/ai/skills/reuse-star-stroot.md`](../docs/ai/skills/reuse-star-stroot.md)
- [`plan_kfparticle_pro_integration.md`](plan_kfparticle_pro_integration.md)

## Non-regression requirements

The following existing files must not be edited, replaced, renamed, moved, or
deleted:

```text
StMaker/StLambdaMaker/
analysis/anaLambda.C
analysis/run_anaLambda.C
script/run_anaLambda.sh
```

The existing `StLambda` data type, existing library ABI, existing mainconf
requirements, and existing analysis entry points must remain unchanged.

KFParticle is an additive extension. The normal complete build includes the
new components, but existing analyses remain runtime-independent from them:

- `make all` builds the existing stack and the KFParticle stack;
- `make core` builds only the existing KF-independent stack;
- `libStCommon.so` and existing Maker libraries must not acquire KFParticle
  include flags, link flags, or `DT_NEEDED` entries;
- existing run macros must not load KFParticle libraries;
- a mainconf without a `kf:` key must behave exactly as before.

This separates two concerns: KFParticle is available after the ordinary build,
while an existing job does not load or require it at runtime.

## Scope

### In scope

- Vendor the required KFParticle sources into this repository.
- Build repository-local KFParticle libraries with the existing toolchain.
- Convert PicoDst global tracks and `TrackCovMatrix` into KFParticle input.
- Reconstruct Lambda and anti-Lambda candidates with YAML-controlled cuts.
- Add a separate `StLambdaKFParticleMaker` and KF-only helper library.
- Add KF-specific macros, configurations, run script, and QA.
- Compare KFParticle and helix reconstruction on identical events.

### Out of scope for the initial implementation

- Modification of `StLambdaMaker` or the existing Lambda macros/scripts.
- Xi/Omega reconstruction and `KFParticlePerformance`.
- Copying `StKFParticleAnalysisMaker` wholesale.
- MuDst, `bfc`, `kfpAna`, `.DEV2` runtime, or `cons` production.
- TMVA, event-plane code, paper-specific trees, or PV refitting.
- Making an existing runtime library depend on KFParticle.

## Architecture

```text
KF-independent build                 Complete/default build
--------------------                 ----------------------
make core                            make all
  libStarAnaConfig.so                  core
  libStRefMultCorr.so                  libKFParticle.so
  libStCommon.so                       libStKfParticleCommon.so
  existing St*Maker libraries         libStLambdaKFParticleMaker.so

Focused development target
--------------------------
make kfparticle-analysis
  base libraries
  KFParticle and KF Lambda libraries
```

Runtime responsibilities:

```text
StChain
  StPicoDstMaker
    Event + Track + TrackCovMatrix + PID traits
  StLambdaKFParticleMaker
    event/centrality selection and output
    |
    +-- libStKfParticleCommon.so / KfParticleHelper
          Pico covariance conversion
          track/PID selection
          PV and magnetic-field setup
          KF reconstruction and candidate conversion
          |
          +-- libKFParticle.so
```

- `StRoot/KFParticle/`: vendored KF math/reconstruction.
- `StMaker/kfparticle/`: KF-only PicoDst adapter.
- `StMaker/StLambdaKFParticleMaker/`: new Lambda Maker.
- `StMaker/common/` and `libStCommon.so`: unchanged KF-independent code.
- `StMaker/StLambdaMaker/`: unchanged helix reference implementation.
- `analysis/*.C`: chain construction and event loop only.
- `config/`: numerical cuts and behavior switches.

## Phase 1: vendor KFParticle

Canonical source:

```text
/star/nfs4/AFS/star/packages/.DEV2/StRoot/KFParticle/
```

Copy the minimum coherent set of headers, sources, and bundled `KFPSimd/` to
`StRoot/KFParticle/`. Do not depend on `.DEV2` at build or runtime, and do not
copy `StKFParticleAnalysisMaker` or `KFParticlePerformance` unless a verified
library dependency requires a small part of them.

Add:

```text
StRoot/KFParticle/PROVENANCE.md
StRoot/KFParticle/README.md
StRoot/KFParticle/COPYING
```

Record the source path, STAR release/library tag, copy date, source file list
or checksums, local modifications, compiler assumptions, build macros, and
license notices.

## Phase 2: build integration

### Target graph

Implement the following logical targets in `Makefile`:

```make
base-libs: libStarAnaConfig libStRefMultCorr libStCommon

core: base-libs $(existing_non_kf_maker_libraries)

kfparticle-analysis: base-libs \
    libKFParticle \
    libStKfParticleCommon \
    libStLambdaKFParticleMaker

all: core kfparticle-analysis
```

Consequences:

- the default Make target remains `all`;
- ordinary `make BUILD_BITS=...` builds KFParticle too;
- the current argument-free `singularity_make.sh` build produces all KF
  libraries without a special target argument;
- `submit.sh --rebuild-if-needed`, whose existing rebuild path runs ordinary
  `make`, also produces the complete build without changing `submit.sh`;
- `make core` remains a KF-independent regression/diagnostic build;
- `make kfparticle-analysis` remains useful for focused development.

### Per-Maker dependencies

Keep the current `StMaker/St*Maker` discovery, but extend the generic Maker
rule to accept per-Maker extra prerequisites, include flags, and link flags.
Conceptually:

```make
MAKER_EXTRA_DEPS_StLambdaKFParticleMaker := \
  $(LIB_DIR)/libKFParticle.so \
  $(LIB_DIR)/libStKfParticleCommon.so
MAKER_EXTRA_CXXFLAGS_StLambdaKFParticleMaker := $(KF_PARTICLE_CXXFLAGS)
MAKER_EXTRA_LDLIBS_StLambdaKFParticleMaker := \
  -lKFParticle -lStKfParticleCommon
```

The variables are empty for every existing Maker, leaving their commands and
dependency graph unchanged. Do not add KF include paths or libraries to global
`CXXFLAGS_MAKER`, global link flags, or generic Maker prerequisites.

### Separate KF helper library

Do not put KF helper sources in `StMaker/common/`, because the current wildcard
would compile them into `libStCommon.so`. Instead add:

```text
StMaker/kfparticle/KfParticleHelper.h
StMaker/kfparticle/KfParticleHelper.cxx
StMaker/kfparticle/README.md
```

Build these files only into `libStKfParticleCommon.so`. Keep the source list and
link dependencies of `libStCommon.so` KF-independent.

### KF libraries

- Define `KFP_DIR := StRoot/KFParticle`.
- List the required KF sources/objects explicitly.
- Use dedicated KFParticle/KFPSimd compile flags.
- Use upstream STAR `__ROOT__` homogeneous-field mode where required.
- Build `lib/libKFParticle.so`.
- Build `lib/libStKfParticleCommon.so` from `StMaker/kfparticle/`.
- Apply KF flags only to `StLambdaKFParticleMaker`.

## Phase 3: backward-compatible configuration

Add:

```text
include/cuts/KfParticleCutConfig.h
src/cuts/KfParticleCutConfig.cpp
config/cuts/kf/kf_auau19_anaLambda_KFParticle.yaml
```

Extend `ConfigManager` additively with an optional `kf:` mainconf key and
`GetKfParticleCuts()`. Loading any existing mainconf without `kf:` must produce
no new failure, warning, or changed default. `KfParticleCutConfig` must contain
no KFParticle classes so `libStarAnaConfig.so` has no KF runtime dependency.

Configure, rather than hard-code:

- reconstruction PDGs (`3122` and optionally `-3122`);
- track hit, hit-ratio, `pT`, `eta`, and covariance requirements;
- proton/pion TPC n-sigma and optional TOF PID;
- multiple-PID-hypothesis policy;
- primary/topological chi-square cuts;
- daughter distance and decay-length cuts;
- mass, mass uncertainty, `chi2/NDF`, and `L/dL` cuts.

## Phase 4: PicoDst-to-KFParticle conversion

`KfParticleHelper` owns conversion and reconstruction. Its interface should
allow initialization from `KfParticleCutConfig`, one call per event, retrieval
of framework-owned Lambda candidate values, and `Clear()`.

For each track:

1. Confirm the corresponding `StPicoTrackCovMatrix` exists.
2. Reject configured `isBadCovMatrix()` entries.
3. Read the six helix parameters from `params()`.
4. Reconstruct the 15-element helix covariance from `sigmas` and
   `correlations` using STAR indexing.
5. Initialize a local `StDcaGeometry`.
6. Obtain Cartesian parameters and the 21-element covariance with `GetXYZ()`.
7. Reject non-finite or invalid parameters/covariance.
8. Populate `KFPTrack` with parameters, covariance, charge, ID, chi-square,
   and NDF.

Do not define `__TFG__VERSION__` globally or call an unavailable SL24y
`StPicoTrackCovMatrix::dcaGeometry()` implementation. Keep KF covariance use
enabled; do not hide conversion errors by disabling it.

Build `KFVertex` from the Pico primary vertex and error, set the event magnetic
field from `StPicoEvent::bField()`, assign proton/pion hypotheses from charge
and configured PID, and reconstruct configured decay PDGs. Resolve daughters
back to Pico track IDs and reject missing, repeated, or incompatible daughters.

Record counters for input tracks, covariance failures, accepted tracks, PID
hypotheses, reconstructed candidates, selected candidates, and conversion
failures.

## Phase 5: create `StLambdaKFParticleMaker`

Create a separate Maker:

```text
StMaker/StLambdaKFParticleMaker/StLambdaKFParticleMaker.h
StMaker/StLambdaKFParticleMaker/StLambdaKFParticleMaker.cxx
StMaker/StLambdaKFParticleMaker/StLambdaKFParticleMakerLinkDef.h
```

Use `StLambdaMaker` only as the reference for Maker lifecycle, event selection,
centrality, histogram ownership, output conventions, and downstream accessors.
Implement `Init()`, `Make()`, `Clear()`, and `Finish()` in the new Maker.

The new Maker calls the helper once per accepted event, applies final YAML
selection, fills QA, exposes candidate momentum/mass/daughter IDs, and writes
ROOT output. Do not modify the existing `StLambda` layout. If KF-specific
persistent values are needed, add a separate value type or dedicated tree.

## Phase 6: KF-only analysis entry points

Add:

```text
analysis/anaLambda_KFParticle.C
analysis/run_anaLambda_KFParticle.C
script/run_anaLambda_KFParticle.sh
```

The analysis macro follows the existing StChain lifecycle, enables
`TrackCovMatrix`, and instantiates `StLambdaKFParticleMaker`. Pairing and fit
logic remain in the Maker/helper.

Only `run_anaLambda_KFParticle.C` loads the KF libraries. Required order:

1. STAR/PicoDst libraries;
2. `libStarAnaConfig.so`;
3. `libStRefMultCorr.so`;
4. `libKFParticle.so`;
5. `libStKfParticleCommon.so`;
6. `libStCommon.so`;
7. `libStLambdaKFParticleMaker.so`.

Do not change any existing runner to load these libraries.

## Phase 7: runnable 19 GeV configuration

Add separate files rather than overwriting existing Lambda configurations:

```text
config/mainconf/main_auau19_anaLambda_KFParticle.yaml
config/analysis/analysis_info_auau19_anaLambda_KFParticle.yaml
config/cuts/kf/kf_auau19_anaLambda_KFParticle.yaml
config/hist/hist_auau19_anaLambda_KFParticle.yaml
```

Reuse unchanged event, centrality, and dataset configs. Synchronize `anaName`,
macro names, job name, scratch directory, and output stem. Keep C++ independent
of a specific mainconf so FXT configuration can be added later.

Do not modify `job/run/submit.sh`: its existing default `make` now builds the
complete target, while non-KF jobs still load only their original libraries.

## Phase 8: QA

Preserve compatible helix histogram names/axes for mass, kinematics, decay
length, centrality, PID, event QA, and centrality QA. Add KF QA for covariance
rejections, accepted inputs, PID hypotheses, candidate counts, mass uncertainty,
fit `chi2/NDF`, decay-length significance, topology deviation, decay vertex,
and mass versus principal KF quality variables.

## Phase 9: validation

### Protected-file gate

Require zero diff for:

```text
StMaker/StLambdaMaker/
analysis/anaLambda.C
analysis/run_anaLambda.C
script/run_anaLambda.sh
```

### Clean builds

Run in the batch-matched toolchain:

```bash
make clean
make core

make clean
make all
```

Then run the ordinary wrapper without an extra target:

```bash
./script/singularity_make.sh \
  config/mainconf/main_auau19_anaLambda_KFParticle.yaml
```

Confirm that `make core` does not compile/link KFParticle and that `make all`
and the wrapper produce `libKFParticle.so`, `libStKfParticleCommon.so`, and
`libStLambdaKFParticleMaker.so`.

### Runtime dependency isolation

Use `readelf -d` or `ldd` to prove:

- `libStCommon.so` has no KF `NEEDED` entry;
- representative existing Maker libraries have no KF `NEEDED` entry;
- `libStLambdaKFParticleMaker.so` alone requires KFParticle and the KF helper.

### Existing-analysis regression

- Load representative existing mainconfs without `kf:`.
- Run a short existing `anaLambda` job and one representative non-Lambda job.
- Confirm their runners do not load KFParticle.
- Compare event count, major histogram entries, and exit status before/after.
- Confirm the same existing jobs can run from a `make core` build.

### KF short run and comparison

- Run the KF macro from the ordinary `make all` output.
- Verify `TrackCovMatrix`, rejection counters, no NaN/Inf, histograms,
  `chain->Finish()`, and clean exit.
- Check that Lambda/anti-Lambda mass peaks appear at plausible positions.
- Run helix and KF methods on identical events and compare event/candidate
  counts, mass/sidebands, kinematics, decay length, and topology variables.
  Candidate counts need not be identical.

## Documentation

Update `docs/REFERENCE.md`, and update `INSTALL.md` if first-build requirements
change. Document the helper API in `StMaker/kfparticle/README.md`. Change
submission documentation only if submission behavior beyond the existing
default build is changed.

## Completion criteria

1. Existing `StLambdaMaker`, Lambda macros, and Lambda run script have zero
   diff.
2. Clean `make all` builds both the existing stack and all KF libraries.
3. Ordinary `singularity_make.sh` and the existing submit rebuild path perform
   that complete build without a special target.
4. Clean `make core` builds the existing stack without KFParticle.
5. Existing libraries and runners have no KFParticle runtime dependency.
6. Existing mainconfs without `kf:` retain identical behavior.
7. Existing Lambda and representative non-Lambda analyses build and run.
8. The separate `StLambdaKFParticleMaker` builds through both `make all` and
   `make kfparticle-analysis`.
9. Pico covariance conversion works without `__TFG__VERSION__`.
10. The KF macro reconstructs PDG 3122 candidates and writes KF QA.
11. All cuts come from the selected mainconf and referenced YAML.
12. Same-input helix-versus-KF comparison and non-regression results are
    documented.

## Implementation status (2026-09-04)

The additive implementation described above is now present. The protected
helix-analysis files have zero diff, and `StLambdaKFParticleMaker` is a new,
separate Maker.

Validated in the SL24y Singularity environment:

- clean `make core` succeeds and produces no KFParticle libraries;
- clean ordinary `make all` succeeds and produces `libKFParticle.so`,
  `libStKfParticleCommon.so`, and `libStLambdaKFParticleMaker.so`;
- `readelf -d` shows no KF dependency in `libStCommon.so`,
  `libStLambdaMaker.so`, or `libStPhiMaker.so`;
- only the KF Maker/helper libraries carry KF `DT_NEEDED` entries;
- both the new and existing Lambda macros ACLiC-compile, load their respective
  mainconfs, initialize StChain, and exit cleanly;
- the KF macro enables the PicoDst `TrackCovMatrix` branch;
- the generated SUMS file is
  `job/joblist/joblist_auau19_anaLambda_KFParticle.xml`.

The available `config/picoDstList/test.list` points to `/home/starlib` files
that are not mounted/present on the validation node. Consequently, the runtime
smoke tests reached clean `Finish()` with zero input entries; candidate-level
mass-peak and same-event helix-versus-KF comparisons remain to be performed on
a node with accessible PicoDst input.
