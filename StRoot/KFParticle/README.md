# Vendored full KFParticle reconstruction core

This directory supplies a coherent `.DEV2` snapshot of the scalar and SIMD
KF classes, PV reconstruction, `KFParticleFinder` and
`KFParticleTopoReconstructor`. It builds as `lib/libKFParticle.so` and is
used by the event-level PicoDst adapter under `StMaker/kfparticle/`.
It is no longer the 9月4日 four-source scalar pair-fit subset.

All imported C++ types are in `star_analyzer_kfp`; for example:

```cpp
#include "KFParticleTopoReconstructor.h"
namespace kfp = star_analyzer_kfp;
// kfp::KFParticleTopoReconstructor, kfp::KFParticle, kfp::KFPTrack, ...
```

Compile every implementation using these internal types with the Makefile's
`KFP_ABI_FLAGS`. Do not expose SIMD headers to CINT or place external headers
inside a namespace. The namespaced snapshot can coexist with SL24y's old
global `StarRoot` KFParticle types; the two are not ABI-compatible.

Build in the configured SL24y Singularity environment:

```bash
./script/singularity_make.sh config/mainconf/main_auau19_anaLambda_KFParticle.yaml all
./script/singularity_make.sh config/mainconf/main_auau19_anaLambda_KFParticle.yaml --no-clean test-kfparticle-full-chain
```

Available targets are `all` (existing analyses plus full KF analysis), `core`
(existing analyses only), and `kfparticle-analysis` (KF analysis and shared
base libraries). The wrapper cleans by default; use `--no-clean` for an
incremental build when appropriate.

Load normal STAR dependencies, including `StarRoot` and `StBichsel`, then the
project libraries in dependency order: `libKFParticle.so`,
`libStKfParticleCommon.so`, and `libStLambdaKFParticleMaker.so`. The dedicated
KF runner handles this; existing Helix runners do not load the new KF stack.
Use a fresh ROOT process after ABI-changing rebuilds.

The adapter passes the event's signed `bField()` before any KF calculation,
then PID hypotheses, input PV and primary-track indices to Topo. Full
reconstruction calls `SortTracks()` and `ReconstructParticles()`; Λ results
are read from `GetParticles()` with PDG ±3122, not rebuilt by manual pairs.
The standard Topo selection already applies a PV compatibility cut to Λ.
The parent mass used for the spectrum is unconstrained; that does not mean
the candidate was selected without topology cuts.

See [`PROVENANCE.md`](PROVENANCE.md) for definitions, preserved upstream cuts,
namespace transformation and limitations; [`SNAPSHOT.json`](SNAPSHOT.json)
records every source and transformed checksum. The import is reproducible
with `python3 tools/kfparticle/import_core.py --check` from the project root.
