# Skill: add-new-analysis

Adds a new analysis with Maker, macros, run script, config, and joblist.

## Required outputs

1. `StMaker/StXXXMaker/` implementation (`.h` / `.cxx`; directory name must match class name so `make` auto-builds `lib/libStXXXMaker.so`)
2. Shared helpers (if any) under `StMaker/common/` → `lib/libStCommon.so` (no Makefile edit)
3. `analysis/run_anaXxx.C` (Load/link `libStCommon` before the Maker `.so`) and `analysis/anaXxx.C`
4. `script/run_anaXxx.sh`
5. `config/mainconf/main_<anaName>.yaml` plus referenced YAML files
6. `config/analysis/analysis_info_*.yaml` with synchronized fields
7. `job/joblist/joblist_<anaName>.xml` from `script/generate_joblist.sh` (`analysis.anaName`)

Do **not** add a hand-written Makefile target for a new `St*Maker`; discovery is automatic.

For local validation on hosts where host `make` or `root4star` is unreliable — or whenever you want an **SL7-class / batch-matched build** without interactive `sl7` — use **`./script/singularity_make.sh <mainconf>`** and the other wrappers in `singularity-local-build-run.md` with the same `mainconf`.

## Rules

- Keep event loop in chain pattern only.
- Keep physics/cut logic in Maker, not runner macro.
- Prefer existing naming conventions and YAML aliases to keep `anaName` consistent.
- If scripts/workflow changed, follow `update-readme-scripts.md`.
- If the analysis needs STAR or past-analysis code from `$STAR/StRoot`, `papers/`, or `others/`, follow [`reuse-star-stroot.md`](reuse-star-stroot.md) before vendoring into `StRoot/`.

## Reference

- `../../../docs/REFERENCE.md`
- `../../../PHILOSOPHY.md`
