# STAR analysis framework

This repository is a **PicoDst analysis framework** for STAR. It follows the **StChain / StMaker** pattern: a ROOT macro builds a chain and drives `Init` → `Make(i)` → `Finish`, while physics, histograms, and cut-driven logic live in **compiled Makers** (`lib/libStXXXMaker.so`). **YAML mainconf** files (`config/mainconf/main_<anaName>.yaml`) are the single entry point for configuration; `script/setup.sh` reads `analysis_info` through that mainconf, and batch joblists embed the same mainconf path that you pass to `script/generate_joblist.sh`. For new analyses, `script/setup_config_from_analysisinfo.py` bootstraps `main_<anaName>.yaml` from `config/mainconf/mainconf.yaml` using the `__ANANAME__` placeholder. You can run locally with `root4star` or submit **star-submit** jobs using generated XML under `job/joblist/`.

**Farm-matched build:** `./script/singularity_make.sh config/mainconf/main_<anaName>.yaml` runs `make` inside the same **`star-bnl/star-sw:latest`** Singularity image as batch (SL7-class `sl73_*` toolchain). Use it when you are not in interactive `sl7` or when host `make` does not match STAR. Details: [INSTALL.md](INSTALL.md) Step 6 and [docs/REFERENCE.md](docs/REFERENCE.md) (Local with Singularity).

## At a glance

```mermaid
flowchart TD
  R4["root4star"]
  RS["script/run_anaXxx.sh (optional wrapper)"]
  Run["analysis/run_anaXxx.C<br/>(load libs, .L anaXxx.C+)"]
  Ana["analysis/anaXxx.C<br/>(compiled by ACLiC)"]

  MC["config/mainconf/main_*.yaml"]
  CFG["ConfigManager::LoadConfig(mainconf)"]
  CUTC["config/cuts/*.yaml"]
  MKC["config/maker/*.yaml"]
  HC["config/hist/*.yaml"]
  AIC["config/analysis/analysis_info*.yaml"]

  Ch["StChain"]
  P["StPicoDstMaker"]
  M["StLambdaMaker / StPhiMaker"]

  RS --> R4
  R4 --> Run
  Run --> Ana

  Ana --> CFG
  MC --> CFG
  MC --> CUTC
  MC --> MKC
  MC --> HC
  MC --> AIC

  Ana --> Ch
  Ch --> P
  Ch --> M
```

**In short:** `run_anaXxx.C` loads STAR/project libraries, compiles `anaXxx.C+`, and calls the analysis function; `anaXxx.C` loads mainconf through `ConfigManager`, builds `StChain` (`StPicoDstMaker` + analysis Maker), and runs the event loop. **Why two macros?** See [PHILOSOPHY.md](PHILOSOPHY.md).

## Documentation

| Document | Purpose |
|----------|---------|
| [INSTALL.md](INSTALL.md) | First-time setup: clone, submodule, directories, analysis_info, build, local run (and optional batch). |
| [PHILOSOPHY.md](PHILOSOPHY.md) | Design principles, reproducibility, two-macro rationale; **source of truth** for agents and contributors. |
| [docs/ai/README.md](docs/ai/README.md) | AI guidance map. Includes `AGENT_RULES.md`, task skills, and migration policy. |
| [CLAUDE.md](CLAUDE.md) | Antigravity / Claude Code entrypoint. Points to shared source-of-truth docs. |
| [docs/REFERENCE.md](docs/REFERENCE.md) | Full reference: prerequisites, analysis_info tables, how to run, batch, QA scripts, adding an analysis, config, joblists, and submit-time reproducibility artifacts. |
| [job/run/README.md](job/run/README.md) | Submit, `configlog`, `runmeta`, and cleaning up `job/run/`. |

Only **template/sample** content under `config/` and `job/joblist/` is tracked in git; built `.so` files under `lib/` are git-ignored.

Successful `job/run/submit.sh` submissions now save per-`jobid` reproducibility artifacts under `job/run/`: the submitted XML snapshot, the resolved config snapshot, and a `runmeta` manifest with git/code-state sidecars and a tarball of SUMS-generated submit artifacts. See [job/run/README.md](job/run/README.md) and [docs/REFERENCE.md](docs/REFERENCE.md) for paths and details.

## Repository layout (short)

| Path | Role |
|------|------|
| **analysis/** | `run_anaXxx.C` (runner) + `anaXxx.C` (chain; ACLiC). One pair per analysis. |
| **config/** | `mainconf/`, `cuts/`, `maker/`, `hist/`, `analysis/` (analysis_info), `picoDstList/`. |
| **StMaker/** | Maker sources (`St*Maker/`) → `lib/libStXXXMaker.so`; shared helpers in `common/` → `lib/libStCommon.so`. |
| **script/** | `setup.sh`, `run_ana*.sh`, `run_fitCorrelation.sh` (LL/KP femto fit helper for ROOT CF histograms), `singularity_make.sh`, `singularity_run_ana*.sh`, `generate_joblist.sh`, `checkHistAnaPhi.sh`, `checkHistAnaLambda.sh`, `singularity_checkHistAnaPhi.sh`, `singularity_checkHistAnaLambda.sh`, helpers. |
| **job/** | `joblist/` templates; `job/run/` for submit and logs. |
| **include/** / **src/** | Framework (`ConfigManager`, cuts, yaml-cpp build). |

See [docs/REFERENCE.md](docs/REFERENCE.md) for a longer directory table and script list.

## Development with Cursor (optional)

Open this repository as the **project folder** in [Cursor](https://cursor.com/). Canonical AI guidance lives in [docs/ai/README.md](docs/ai/README.md) and [PHILOSOPHY.md](PHILOSOPHY.md). [.cursor/rules/](.cursor/rules/) and [.cursor/skills/](.cursor/skills/) are lightweight wrappers for Cursor integration.

When a skill source under `docs/ai/skills/*.md` is edited in Cursor, `.cursor/hooks.json` triggers `.cursor/hooks/auto-sync-skills.sh`, which runs `script/sync_and_check_skills.sh` automatically.
