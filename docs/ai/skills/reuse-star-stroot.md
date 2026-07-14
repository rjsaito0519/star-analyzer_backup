# Skill: reuse-star-stroot

Use when implementing or extending analysis features and you need to find reusable STAR code in `$STAR/StRoot`, past analysis trees (`papers/`, `others/`), or decide whether to vendor code into this repo's `StRoot/`.

## Canonical vendoring example

**[`StRoot/StRefMultCorr/`](../../../StRoot/StRefMultCorr/)** — follow this layout when vendoring:

- [`PROVENANCE.md`](../../../StRoot/StRefMultCorr/PROVENANCE.md) — copy source, date, starver, modifications
- [`README.md`](../../../StRoot/StRefMultCorr/README.md) — API, load order, pitfalls
- `Makefile` → `lib/libStRefMultCorr.so` (see `RMC_DIR` in root `Makefile`)
- `analysis/run_anaPhi.C` — loads repo `lib/libStRefMultCorr.so` **before** Maker libs, not `$STAR/lib/libStRefMultCorr.so`
- Thin framework wrapper: [`StMaker/common/CentralityHelper.h`](../../../StMaker/common/CentralityHelper.h)

For centrality **usage** after vendoring, use [`centrality-strefmultcorr.md`](centrality-strefmultcorr.md) (do not duplicate bin tables here).

---

## Part A: Discovery and reference search

### 1. Confirm environment

```bash
echo $STAR
echo $STAR_HOST_SYS
# From mainconf → analysis_info:
grep -E 'libraryTag|anaName' config/analysis/analysis_info_*.yaml
```

Use the same `libraryTag` / `starver` as the analysis you are building or debugging.

### 2. Search `$STAR/StRoot` (PicoDst-oriented)

Start with paths this repo already uses (see root `Makefile` `STAR_INC` and `analysis/run_ana*.C`):

| Path under `$STAR/StRoot` | Typical use |
|---------------------------|-------------|
| `StPicoDstMaker`, `StPicoEvent` | PicoDst I/O (usually via `$STAR/lib`, not vendored) |
| `StMaker`, `StarClassLibrary` | Chain / track base classes |
| `StBTofUtil` | TOF matching utilities |
| `StMuDSTMaker/COMMON/macros/loadSharedLibraries.C` | Standard STAR lib load macro |

Example searches (replace `Keyword` with class or function name):

```bash
rg -l 'Keyword' "$STAR/StRoot" --glob '*.{h,cxx,cpp,C}'
find "$STAR/StRoot" -maxdepth 2 -type d -iname '*keyword*'
```

### 3. Search this repo and local analysis trees

```bash
# Already vendored
ls StRoot/

# Past / parallel analyses (examples referenced in docs)
rg -l 'Keyword' papers/ others/ 2>/dev/null --glob '*.{h,cxx,cpp,C}'
```

See also planning notes that cite external StRoot code, e.g. [`docs/plan_phi_pid_matching.md`](../../plan_phi_pid_matching.md) (`others/femtoPPhi/StRoot/...`).

### 4. Search compiled STAR libs first

Before vendoring, check whether `$STAR/lib` already provides the functionality:

```bash
ls "$STAR/lib" | rg -i 'keyword'
```

If a matching `.so` exists and loads cleanly via `loadSharedLibraries.C` or `gSystem->Load`, prefer that over copying source.

---

## Part B: Decision tree (use in order)

```
1. Does $STAR/lib already provide a .so?
   → YES: use it from run_ana*.C / Makefile STAR_LDFLAGS. Stop (no vendoring).

2. Do you only need an algorithm pattern or API reference?
   → YES: implement in StMaker/<YourMaker>/ or StMaker/common/.
          Do not copy a full STAR Maker or event loop.

3. Do you need STAR / papers code as-is (or with minimal edits)?
   → YES: follow Part C vendoring checklist.

4. Is it analysis physics, cuts, or histograms?
   → YES: StMaker + YAML config (PHILOSOPHY.md). Not StRoot/.
```

**Placement rule:** `StRoot/<Package>/` is for **vendored third-party or STAR utility libraries** (not `StMaker` subclasses). Analysis Makers live under `StMaker/`.

---

## Part C: Vendoring checklist

Complete in order. Use `StRoot/StRefMultCorr/` as the template.

1. **Scope** — Copy the minimum file set needed. Do not vendor entire `$STAR/StRoot` trees or unrelated packages.

2. **Layout** — Create `StRoot/<PackageName>/` with sources and headers only (plus optional reference `.C` macro).

3. **`PROVENANCE.md`** — Record:
   - Source absolute path (STAR tree, `papers/...`, etc.)
   - Copy date
   - `libraryTag` / starver used when copying
   - File list
   - Modifications from upstream (e.g. stripped `ClassDef`, parameter tweaks)
   - Known limitations (run ranges, duplicate-symbol warnings)

4. **`README.md`** (when non-obvious) — API summary, load order, integration with Makers, pitfalls.

5. **`Makefile`** — Add a shared-library target, e.g. `lib/lib<Package>.so`:
   - `RMC_DIR`-style variable: `PKG_DIR := StRoot/<PackageName>`
   - `CXXFLAGS` include `-IStRoot -I$(PKG_DIR)`
   - List only required `.cxx` sources
   - Update dependent Maker targets (`CXXFLAGS_MAKER`, link order) if Makers include headers from the package

6. **`run_anaXxx.C`** — Load repo library from `$PWD/lib/`:
   - Load **before** dependent Maker `.so` if the Maker links against it
   - Add to `gSystem->AddLinkedLibs(...)` for ACLiC
   - **Never** load the same library from both `$PWD/lib/` and `$STAR/lib/` (duplicate symbols)

7. **Code adjustments**
   - Strip or avoid `ClassDef` / ROOT dictionary if the code is only called from compiled C++ (see `StRefMultCorr` notes)
   - Keep cuts and thresholds in YAML, not in vendored code

8. **Framework wrapper** — Prefer a thin helper in `StMaker/common/` (like `CentralityHelper`) so Makers stay readable and YAML-driven.

9. **Build verify**

   ```bash
   ./script/singularity_make.sh config/mainconf/main_<anaName>.yaml
   ```

10. **Docs**
    - Add a short entry in [`docs/REFERENCE.md`](../../REFERENCE.md) near the StRoot / centrality section
    - If domain-specific operation is non-trivial, add or extend a dedicated skill (e.g. `centrality-strefmultcorr.md`)

---

## Do not

- Copy custom event loops or entire STAR `StMaker` analysis chains into `StRoot/`
- Vendor when `$STAR/lib` already provides the same `.so`
- Hardcode analysis cuts or thresholds in vendored code (use YAML via `ConfigManager`)
- Load the same library from repo `lib/` and `$STAR/lib/`
- Duplicate centrality bin tables or other canonical docs — link to `StRoot/StRefMultCorr/README.md` instead
- Vendor large binary assets without documenting them in `PROVENANCE.md` (correction ROOT files belong under `StRoot/<pkg>/` or a documented path; see `docs/plan_phi_kaon_nsigma_correction.md`)

---

## Related skills

- [`add-new-analysis.md`](add-new-analysis.md) — new analysis skeleton; link here when external STAR code is needed
- [`centrality-strefmultcorr.md`](centrality-strefmultcorr.md) — operating vendored `StRefMultCorr` after integration
- [`singularity-local-build-run.md`](singularity-local-build-run.md) — batch-matched build for vendored libs

## Reference

- [`PHILOSOPHY.md`](../../../PHILOSOPHY.md) — physics in Makers, YAML-driven config
- [`docs/REFERENCE.md`](../../REFERENCE.md) — build, load order, StRoot overview
- [`docs/ai/AGENT_RULES.md`](../AGENT_RULES.md) — editing rules, skill sync
