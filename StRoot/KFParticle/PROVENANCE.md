# KFParticle full reconstruction core provenance

- Source: `/star/nfs4/AFS/star/packages/.DEV2/StRoot/KFParticle/`
- Snapshot date: 2026-09-07; replaces the 2026-09-04 scalar-only import.
- Target toolchain: STAR `SL24y`, `sl73_x8664_gcc485`, ROOT 5.34/38, GCC 4.8.5.
- Purpose: the standard SIMD → Finder → Topo reconstruction engine, called
  through the project's PicoDst Interface port, for Λ and anti-Λ selection.
- Exact copied and transformed file hashes: [`SNAPSHOT.json`](SNAPSHOT.json).
  Its aggregate `source_id` is also compiled into
  [`KFParticleSnapshot.h`](KFParticleSnapshot.h).

## Coherent source closure

The following 11 implementations and their headers all come from this one
snapshot; no old `KFParticleBase` / Vc implementation is mixed in:

```text
KFParticle                 KFPTrack                   KFPVertex
KFParticleDatabase         KFVertex                   KFPTrackVector
KFPEmcCluster              KFParticleSIMD             KFParticlePVReconstructor
KFParticleFinder           KFParticleTopoReconstructor
```

Additional dependencies are `KFParticleDef.h`, `KFParticleMath.h`,
`KFParticleField.h`, `KFPInputData.h`, `KFPSimdAllocator.h`, and the bundled
`KFPSimd` headers. `KFPEmcCluster` is a compile dependency, not a request to run
EMC reconstruction in the Λ analysis. No `Performance`, MC/embedding I/O,
`StKFParticleAnalysisMaker`, TPC tracker, SCIF or TMVA programs are built here.

The old scalar import's four implementations are reimported together with the
seven additions, not retained as an unverified different version.

## Reproducible local namespace patch

[`tools/kfparticle/import_core.py`](../../tools/kfparticle/import_core.py)
performs the import and can verify it without modifying files:

```bash
python3 tools/kfparticle/import_core.py --check
```

Paths in that command are relative to the project root. Re-import is explicit,
not a build-time download. The script refuses to overwrite unrecorded local
changes. An updated upstream snapshot must be reviewed as a new dependency.

These local compatibility changes are applied to C++ sources:

1. Put declarations, definitions, helper functions, allocator and bundled SIMD
   namespaces inside `star_analyzer_kfp`.
2. Close that namespace before each `#include` and reopen immediately after the
   include in the same preprocessor branch. External ROOT, STL, intrinsics and
   STAR headers therefore remain in the global namespace.
3. Prefix each vendor header guard with `STAR_ANALYZER_VENDOR_` to avoid the
   old `StarRoot/KFParticle.h` guard suppressing the new declarations.
4. Guard `KFPTrack`'s `ClassDef` with
   `defined(__ROOT__) && !defined(KFParticleStandalone)`, matching the other
   internal KF types' standalone dictionary opt-out. The upstream KFPTrack
   guard tests only `__ROOT__`, which otherwise leaves ROOT dictionary symbols
   undefined at link time. Its TObject inheritance and all STAR data members
   remain intact. This patch changes reflection only, not reconstruction.

The arithmetic, fit algorithm, upstream cuts, order of operations and STAR
conditional branches are not rewritten. Keep the transformation script and
manifest in sync rather than editing a generated vendor file by hand.

Why this is needed: SL24y `StarRoot.so` contains an incompatible, global old
`KFParticle` implementation, including an 8-byte `KFParticle::fgBz`; the
new snapshot has a 4-byte field. Renaming just the `.so` or reordering loads
does not prevent ELF symbol preemption. The new core exports only namespaced
KF symbols; old `StarRoot` remains available for existing analyses and for
`TRVector` / `TRSymMatrix` used by the new core's STAR diagnostic methods.

## Build definitions and linkage

All KF core, KF adapter and KF Maker implementation units use:

```text
-std=c++11 -msse4.1 -D__ROOT__ -DKFParticleStandalone -DHomogeneousField=
```

- `__ROOT__` preserves STAR-specific layouts and reconstruction conditions.
  It is not a ROOT major-version requirement.
- `KFParticleStandalone` suppresses internal ROOT dictionaries; it does not
  remove Finder, Topo or SIMD. These classes still use ROOT 5 base facilities.
- `HomogeneousField` is defined to an empty value, like upstream's `__ROOT__`
  branch, to avoid a macro-redefinition warning. It uses the event's signed `bField()` convention. The adapter
  sets the field before constructing PID hypotheses and PV deviations.
- The snapshot's active SIMD path is bundled KFPSimd SSE4.1. No external Vc,
  `-march=native`, SIMD TPC tracker or ROOT 6 is selected.

The core links ROOT and SL24y `StarRoot` for STAR diagnostic matrix classes.
It also explicitly links `Table`, `Geom` and `EG`, like STAR's `rootlogon.C`,
because this SL24y `StarRoot.so` does not encode its ROOT dependencies in
`DT_NEEDED`. This resolves the standalone test's TDataSet/TVolume/TCL/PDG
symbols without suppressing checks on references from the new core.
Only the KF-specific adapter additionally links `StBichsel` for the SL24y
`StdEdxPull` port. KF flags and these new dependencies are not added to
`libStCommon.so`, existing Makers or the `core` target.

`all: core kfparticle-analysis` is maintained. Header and Makefile dependencies
rebuild all KF ABI consumers consistently; compiler-generated dependencies
track indirect includes. A new ROOT process must be used after rebuilding
this ABI, not a process still holding the 9月4日 scalar library.

## Upstream selection conditions deliberately preserved

These are engine conditions, not newly invented analysis cuts. Source line
numbers below refer to the untransformed upstream snapshot.

- `KFParticleFinder.cxx`, two-body path near line 816: positive finite fit χ²,
  the internal 200 cm `lMin` bound and geometric/PV compatibility conditions.
- Near line 868: database peak sigma used for mass-window classification is
  different from each candidate's fitted mass error.
- Near line 1056, `__ROOT__` branch: secondary candidate topology condition
  `chi2TopoMin < 500` is active.
- `KFParticleTopoReconstructor::ReconstructParticles()` calls
  `SelectParticleCandidates()`. Its active `UseParticleInCompetition` species
  include both Λ signs. Each retained Λ must have a copy with
  `SetProductionVertex(PV)` and `Chi2()/NDF() < 3` for at least one PV; a
  candidate failing that requirement is marked for deletion. The older
  mass-based candidate-competition block is disabled with `#if 0`.

Consequently `GetParticles()` after the full Topo call is already an
upstream-topology-selected collection. Its Λ mass has not been constrained to
the parent PDG mass, but it must not be described as a topology-uncut inclusive
sample. Loosening a later YAML cut cannot undo the upstream χ²/NDF < 3 step.

## Verification and limitations

The installed SL24y `StarRoot.so` also has a pre-existing undefined
`THelix3d::operator=(const THelix3d&)`: its source header declares it but
`THelix3d.cxx` provides no definition. It is unrelated to the KF path exercised
here, but prevents a fully linked standalone executable against that library.
We do not patch STAR, invent that method or add `--allow-shlib-undefined`.
Tests are compiled as shared modules with unique exported C entry points and
`--no-undefined` for their own references, then run through
`tests/bootstrap_kfparticle_tests.C` in the ordinary `root4star` environment.
The bootstrap compiles `tests/run_kfparticle_tests.C` with ACLiC in a unique
temporary build directory, then invokes its ROOT-visible function. Dynamic
function pointers are handled by compiled C++, not CINT. This also avoids a
shared ACLiC cache when the two make test targets run in parallel. The runner
loads old `StarRoot` before the isolated new core. This uses the same
ROOT-native library path as the production compiled Maker. Test recipes set
`ROOTSYS` and prepend repository, STAR and matching `root-config --libdir`
paths to `LD_LIBRARY_PATH`. `singularity_make.sh` transfers the host setup's
complete loader path in `KF_TEST_RUNTIME_LIBRARY_PATH`; only test recipes use
that saved value (or the current loader path for interactive builds). The
wrapper's original compiler environment still uses its STAR-only loader path.
This mirrors the working analysis run wrapper without enumerating dependencies
such as mysql one at a time. `bash -n script/singularity_make.sh` passed after
the scoped change. Its SHA256 changed from
`7d7d0d882372503f386492810a0f4d53d0957b8ae6d53b4f1ccf1a40de1abc12` to
`a1e3b7593332871395ede797748c750710191fe4c91981a6f2c7ccb4b91d859e`.

The test runner uses ROOT 5's `ResetSignal(..., kTRUE)` for SIGSEGV, SIGBUS,
SIGILL and SIGFPE. Fatal faults must terminate the test process rather than
return to CINT and make an incomplete run appear successful. This is scoped
to the dedicated KF test process and does not alter existing analysis macros.
It is not a claim
that every unused method in the installed STAR library is link-complete.

`tests/kfparticle_full_chain.cxx` / `make test-kfparticle-full-chain` exercises
the genuine Topo/Finder path with synthetic displaced Λ and anti-Λ daughters,
STAR's old library present, noncontiguous IDs, multiple PID hypotheses and
event/field resets. `tests/kfparticle_pico_adapter.cxx` /
`make test-kfparticle-pico-adapter` additionally exercises the adapter with
actual SL24y Pico classes; these synthetic objects are not real data.
Actual outcomes belong in the dated implementation log;
the existence of a test target alone is not a successful validation claim.

Physics validation still requires accessible nonzero PicoDst input,
`TrackCovMatrix`, calibrated PID/cuts appropriate for that data, and comparison
with the reference Interface. Synthetic tests and the 9月4日 zero-event
Init/Finish check cannot establish Λ efficiency or a real-data mass peak.

## License

KFParticle identifies its license as GPL-3.0-or-later. The existing upstream
GPLv3 text remains in `COPYING`; the bundled SIMD license is retained at
`KFPSimd/COPYING`. Original source copyright notices are retained.
