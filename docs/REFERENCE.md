# Reference — STAR analysis framework

Operational detail: prerequisites, setup, analysis_info, build, running locally and in batch, QA, adding analyses, config, and joblists.

- **Overview and doc map:** [README.md](../README.md)
- **First-time setup:** [INSTALL.md](../INSTALL.md)
- **Design principles and two-macro rationale:** [PHILOSOPHY.md](../PHILOSOPHY.md)
- **Submit / cleanup on the farm:** [job/run/README.md](../job/run/README.md)

For **why** `run_anaXxx.C` and `anaXxx.C` are separate, see [PHILOSOPHY.md](../PHILOSOPHY.md) (two macros section).

## Directory layout

Only **template/sample** content under `config/` and `job/joblist/` is tracked; built artifacts under `lib/` are git-ignored.

| Directory | Description |
|-----------|-------------|
| **analysis/** | ROOT macros: `run_anaXxx.C` (runner: loads libs, compiles `anaXxx.C+`, calls analysis) and `anaXxx.C` (StChain + event loop). One pair per analysis (e.g. Lambda, Phi). |
| **config/** | YAML configs. **Templates/samples only** tracked. Subdirs: `mainconf/` (main YAML that includes the rest), `maker/`, `hist/`, `cuts/` (event, track, pid, v0reco, mixing), `analysis/` (e.g. **analysis_info_temp.yaml** — used by setup.sh and joblist generator), `picoDstList/` (input file lists; user lists are typically untracked). |
| **include/** | Framework headers: `ConfigManager.h`, `HistManager.h`, cut configs (`cuts/*.h`). Used by StMaker and `src/`. |
| **job/** | Job submission: `job/joblist/` = **template** job XMLs (tracked); `job/run/` = submit directory (`submit.sh`, `cleanup_job_run.sh`, `archive_job_run.sh`, `snapshot_runmeta.sh`, generated/copied files). On successful submit, `submit.sh` saves `job/run/joblistlog/joblist_<anaName>_<jobid>.xml`, `job/run/configlog/config_<anaName>_<jobid>.txt`, and `job/run/runmeta/runmeta_<anaName>_<jobid>.json` plus sidecars (`gitstatus`, `gitdiff`, `gitsubmodules`, `runtime_bundle`, `sums_artifacts`, `submit_stdout`). Files under `job/run/*.xml` and generated job artifacts are git-ignored. |
| **lib/** | Built shared libraries (`libStarAnaConfig.so`, `libStXXXMaker.so`). **Contents git-ignored**; produced by `make`. |
| **StMaker/** | One subdir per Maker (e.g. `StLambdaMaker/`, `StPhiMaker/`, `StFemtoMaker/`). Each has `.h` and `.cxx`; built into `lib/libStXXXMaker.so`. Femto naming rules: `StMaker/StFemtoMaker/README.md`. |
| **script/** | Environment and run scripts: `setup.sh` (bash/zsh setup), `setup.csh` (csh/tcsh setup), `generate_joblist.sh` (joblist XML from mainconf), `singularity_make.sh` (build `lib/` via batch-like singularity runtime), `run_anaLambda.sh`, `run_anaPhi.sh`, `run_anaFemtoPhiProton.sh`, `singularity_run_anaLambda.sh`, `singularity_run_anaPhi.sh`, `singularity_run_anaFemtoPhiProton.sh` (local analysis via the same singularity runtime), `run_fitCorrelation.sh` (runs `share/femtocalc/fitCorrelation.C+` with safe quoting for ROOT file and histogram name), `checkHistAnaPhi.sh`, `checkHistAnaLambda.sh` (QA PDF from Maker output ROOT), `singularity_checkHistAnaPhi.sh`, `singularity_checkHistAnaLambda.sh`, `singularity_checkHistAnaFemtoPhiProton.sh` (QA PDF via the same singularity runtime; **recommended on AL9**), `merge_root_files.csh` (hadd merge of subjob ROOT by jobid), `watch_job_and_merge.sh` (poll batch output and auto-merge; started by `submit.sh --watch-merge`), `analysis_info_helper.py` (libraryTag, joblist generation, embedded-mainconf extraction, watch-merge path helpers), `sync_cursor_skills.py` / `check_cursor_skill_sync.py` (sync and validate `docs/ai/skills/*.md` ↔ `.cursor/skills/*/SKILL.md` parity), `sync_and_check_skills.sh` (single command to run sync + parity check), `time_NYT_to_JST.py` (NY time → JST), `time_now_NY_to_JST.py` (current NY server time → JST), and helpers (e.g. `get_file_list_*.sh`). |

## Prerequisites and setup

- **STAR environment**: use `starver` (e.g. `starver SL24y`). The version is taken from **analysis info** (see below), not hardcoded.
- **Build tools**: ROOT, gcc, and CMake (for yaml-cpp). The framework is intended to be buildable with any starver environment (subject to future development).

**Setup** reads the main config (mainconf) and the analysis info YAML it points to, then prepares `STAR`, `STAR_HOST_SYS`, `PATH`, and `LD_LIBRARY_PATH` for the selected `libraryTag`. From the project root, **source** the setup script that matches your shell:

```bash
source ./script/setup.sh config/mainconf/main_auau19_anaLambda.yaml
```

```csh
source ./script/setup.csh config/mainconf/main_auau19_anaLambda.yaml
```

For another analysis (e.g. Phi):

```bash
source ./script/setup.sh config/mainconf/main_auau19_anaPhi.yaml
```

Do not run `./script/setup.sh ...` as a standalone command; that only affects a child shell. If the helper script or the analysis info is missing or invalid, the setup script exits with an error.

## Python 3 environment (for joblist generation)

Generating a joblist with `./script/generate_joblist.sh` uses **Python 3** and **PyYAML**. Prepare once:

1. **Python 3**  
   Use the system or module `python3` (e.g. `module load python` if your site provides it).

2. **PyYAML**  
   Install for Python 3 (user install if you lack admin rights):
   ```bash
   python3 -m pip install --user pyyaml
   ```

3. **Run the script**  
   From the project root:
   ```bash
   ./script/generate_joblist.sh config/mainconf/main_auau19_anaLambda.yaml
   ```
   The script prefers `python3` when available and writes e.g. `job/joblist/joblist_auau19_anaLambda_temp.xml` (basename is `analysis.anaName`).

## Analysis info (analysis_info_temp.yaml)

The file **config/analysis/analysis_info_temp.yaml** (or the one referenced by your mainconf’s `analysis:` key) holds metadata used by `setup.sh` and by the **joblist generator**. After cloning, copy or edit this file so that paths and tags match your environment.

**Main keys:**

| Section | Keys | Purpose |
|--------|------|--------|
| **starTag** | `libraryTag`, `triggerSets`, `productionTag`, `filetype`, `filenameFilter`, `filenameNotFilter`, `storageExclude`, `storage` | `setup.sh` uses `libraryTag` for `starver`. The rest are used to build the SUMS catalog URL when generating a joblist. **`filenameNotFilter`** maps to `filename!~<value>` (e.g. `adc` excludes ADC streams). **`storage`** maps to `storage=<value>` (e.g. `local`). **`storageExclude`** maps to `storage!=<value>`. |
| **dataset** | `allPicoDstList`, `runRange`, etc. | Dataset description; can be used by run scripts or docs. |
| **analysis** | `anaName`, `name`, `workDir`, `logDir`, `errDir`, `baseRunMacro`, `baseAnaMacro`, `jobName`, `scratchSubdir`, `outputFileStem`, `nFiles`, `maxEvents` | **anaName** is the canonical name for this analysis (see Naming conventions below). Use YAML alias: define `anaName: &anaName "auau19_anaLambda_temp"` and set `name`, `jobName`, `scratchSubdir`, `outputFileStem` to `*anaName` so they stay in sync. **baseRunMacro** / **baseAnaMacro** are the macro base names without `.C` (e.g. `run_anaLambda`, `anaLambda`); the joblist generator builds the run macro as `baseRunMacro + ".C"` and writes **`joblist_<anaName>.xml`**. **workDir** is the base path for generated ROOT outputs. **logDir** and **errDir** are optional stdout/stderr directories; if omitted, joblist generation uses `workDir/log` and `workDir/err`. Batch runtime now stages a scratch-local bundle and no longer depends on the repository living at `workDir`. If `workDir` is omitted, empty, relative, or left at the template placeholder, joblist generation falls back to the current project root. The batch runtime mainconf is not stored here; it is the mainconf you pass to `generate_joblist.sh`. **maxEvents** is optional and controls the 4th `root4star` argument in generated batch joblists (`-1` if omitted). |
| **analyst** | `name`, `institute`, `email` | For documentation. |

**Script that uses it:** `script/analysis_info_helper.py`

- **`--library-tag`**: Reads mainconf → analysis info and prints `starTag.libraryTag`. Used by `setup.sh`. Works without PyYAML (minimal grep fallback).
- **`--generate-joblist`**: Reads mainconf → analysis info and fills **job/joblist/job_template_from_conf.xml**, then writes **`job/joblist/joblist_<anaName>.xml`** using **`analysis.anaName`** for `anaName`. The embedded `__MAINCONF__` path is the same mainconf you pass to the command, and **`analysis.maxEvents`** (if present) is passed to batch `root4star` as the 4th argument. Requires PyYAML; use the **Python 3 environment** above.
- **`--mainconf-from-joblist`**: Reads a generated joblist XML and prints the embedded `config/mainconf/...yaml` path used by batch runtime and by `submit.sh` preflight.
- **`--ensure-batch-dirs JOBLIST`**: Creates stdout, stderr, and ROOT output directories parsed from the joblist; used by `submit.sh` before `star-submit`.
- **Watch-merge helpers** (used by `watch_job_and_merge.sh`):
  - **`--expected-subjobs-from-joblist JOBLIST --job-run-dir DIR --job-prefix PREFIX`**: Prints count of `{PREFIX}_*.list` under `job/run/`.
  - **`--merge-sample-from-joblist JOBLIST --watch-merge-jobid HEX`**: Prints first subjob ROOT path for `merge_root_files.csh`.
  - **`--merge-output-from-joblist JOBLIST --watch-merge-jobid HEX`**: Prints expected `*_merge.root` path.
  - **`--rootfile-dir-from-joblist JOBLIST`**: Prints absolute rootfile output directory.
  - **`--output-stem-from-joblist JOBLIST`**: Parses output stem from `fromScratch`.
  - **`--rootfile-dir-from-joblist JOBLIST --count-subjob-roots --watch-merge-jobid HEX`**: Counts subjob ROOT files (excludes `*_merge.root`).
  - **`--update-runmeta-postprocess RUNMETA_JSON`**: Reads JSON from stdin and writes `postProcess.watchMerge`.

**Config bootstrap script:** `script/setup_config_from_analysisinfo.py`

- Reads `analysis.anaName` (or `analysis.name`) from analysis_info.
- Copies cut/maker templates and writes `config/mainconf/main_<anaName>.yaml`.
- Uses `config/mainconf/mainconf.yaml` as the mainconf template and replaces `__ANANAME__` in the file content.

## AI skill wrapper synchronization

To keep Antigravity (source docs) and Cursor wrappers aligned, treat `docs/ai/skills/*.md` as source-of-truth.

In Cursor, project hook config at `.cursor/hooks.json` runs `.cursor/hooks/auto-sync-skills.sh` after file edits and automatically syncs when a matching path under `docs/ai/skills/*.md` changes.

Single manual command:

```bash
script/sync_and_check_skills.sh
```

- `sync_and_check_skills.sh` runs sync then parity check.
- `sync_cursor_skills.py` writes/updates `.cursor/skills/<skill>/SKILL.md` wrappers from docs sources.
- `check_cursor_skill_sync.py` exits non-zero when wrappers and docs sources are out of sync (useful for CI/pre-commit).

### Naming conventions

- **anaName** = `{system}_{anaId}[_condition]` (e.g. `auau19_anaLambda`, `auau19_anaLambda_mid`). Define it once per analysis and tie output files, mainconf, and analysis_info to it in a 1:1 way. Use a YAML anchor in analysis_info: `anaName: &anaName "auau19_anaLambda_temp"` and reference it with `*anaName` for `name`, `jobName`, `scratchSubdir`, `outputFileStem`.
- **baseRunMacro** / **baseAnaMacro**: Macro base names without extension (e.g. `run_anaLambda`, `anaLambda`). The runner macro file is `baseRunMacro + ".C"`; the generated joblist file is **`joblist_<anaName>.xml`** (`analysis.anaName`). ACLiC builds `anaLambda_C.so` from the analysis macro.

## Submodules

This repository uses a git submodule for YAML parsing:

- **src/third_party/yaml-cpp** — [jbeder/yaml-cpp](https://github.com/jbeder/yaml-cpp); required to build `libStarAnaConfig.so`.

**After cloning**, initialize and update the submodule:

```bash
git submodule update --init --recursive
```

If you clone with recursive submodules, you can skip the above:

```bash
git clone --recurse-submodules <repository-url>
```

Without the submodule populated, `make` will fail when building the config library.

## Build

From the project root:

```bash
source ./script/setup.sh config/mainconf/main_auau19_anaLambda.yaml
make
```

This builds `lib/libStarAnaConfig.so`, `lib/libStRefMultCorr.so`, `lib/libStCommon.so`, and every `lib/libSt*Maker.so` discovered under `StMaker/St*Maker/` (directory name must match `StXxxMaker/StXxxMaker.cxx`). The Makefile uses `$STAR`, `$STAR_HOST_SYS`, and `root-config`; if `root-config` bitness does not match your setup, the build now fails early with a message telling you to re-source setup and verify `which root-config` / `root-config --cflags`.

## How to run

### Lambda analysis example (local with root4star)

Run from the **project root**:

```bash
./script/run_anaLambda.sh [inputList] [outputRoot] [jobid] [nEvents] [configPath]
```

Defaults:

- `inputList`   = `config/picoDstList/auau19GeV_lambda.list`
- `outputRoot`  = `rootfile/auau19_anaLambda_temp/auau19_anaLambda_temp.root`
- `jobid`       = `0`
- `nEvents`     = `-1` (all events)
- `configPath`  = (default main config; omit to use `config/mainconf/main_auau19_anaLambda.yaml`)

Example (first 100 events):

```bash
./script/run_anaLambda.sh config/picoDstList/auau19GeV_lambda.list rootfile/auau19_anaLambda_temp/out.root 0 100
```

With a custom main config (5th argument):

```bash
./script/run_anaLambda.sh config/picoDstList/auau19GeV_lambda.list rootfile/auau19_anaLambda_temp/out.root 0 -1 config/mainconf/main_auau19_anaLambda.yaml
```

The script sets `LD_LIBRARY_PATH` and runs:

```bash
root4star -b -q "analysis/run_anaLambda.C(\"$INPUT\",\"$OUTPUT\",\"$JOBID\",$NEVENTS[, \"$CONFIG\"])"
```

`run_anaLambda.C` loads STAR libs, `libStarAnaConfig.so`, and `libStLambdaMaker.so`, compiles `anaLambda.C+`, and calls `anaLambda(...)`.

### Local LL/KP correlation fit helper

`share/` is often a symlink (e.g. to `sharelocal` on GPFS). On **AL9**, use the Singularity wrapper so `root4star` runs in an SL7-like runtime and macro paths are resolved with `readlink -f`:

```bash
./script/singularity_run_fitCorrelation.sh <root_file> <mainconf_path> [hist_name]
```

Example:

```bash
./script/singularity_run_fitCorrelation.sh \
  rootfile/auau3p85fxt_anaFemtoPhi4He/auau3p85fxt_anaFemtoPhi4He_<jobid>_merge.root \
  config/mainconf/main_auau3p85fxt_anaFemtoPhi4He.yaml \
  hCF
```

On SL7 (or when host `root` ACLiC works), the plain wrapper avoids quoting mistakes:

```bash
./script/run_fitCorrelation.sh <root_file> [hist_name]
```

This calls `root -b -q "share/femtocalc/fitCorrelation.C+(...)"` and defaults `hist_name` to `hCF`.
Set `STAR_ANA_FIT_ROOT_CMD` to override (example: `STAR_ANA_FIT_ROOT_CMD=root`).

### Local with Singularity (batch-like runtime)

On some login or dev nodes (for example AL9), host `root4star` may fail before the analysis macro runs (missing `libgfortran.so.3`, mixed 32-bit ROOT plugins for `root://`, and similar linker issues). Batch jobs already run `root4star` inside `singularity exec ... star-bnl/star-sw:latest`; use the matching **`singularity_*` wrappers** for local builds and runs on those hosts instead of calling host `root4star` or host `make` directly.

- **Build:** `./script/singularity_make.sh MAINCONF [--no-clean] [make-args...]` — default `make clean && make` with `BUILD_BITS=64`; writes `lib/*.so` under the project root. After `make clean`, CMake is required to rebuild `src/third_party/yaml-cpp` (the wrapper prepends a cvmfs `cmake` when available). **This is the standard SL7-equivalent build:** `make` runs inside `star-bnl/star-sw:latest` with the same `STAR_HOST_SYS` resolution as batch (e.g. `sl73_x8664_gcc485`), so agents and humans can use it instead of an interactive `sl7` session whenever Singularity is available.
- **Lambda:** `./script/singularity_run_anaLambda.sh` — same arguments as `run_anaLambda.sh`.
- **Phi:** `./script/singularity_run_anaPhi.sh MAINCONF [inputFile] [outputFile] [jobid] [nEvents]` — same arguments as `run_anaPhi.sh` (defaults from analysis_info when input/output are omitted).
- **Phi-p femto:** `./script/singularity_run_anaFemtoPhiProton.sh` — same arguments as `run_anaFemtoPhiProton.sh`; uses `StFemtoMaker` (`libStFemtoMaker.so`). Mainconf key **`maker:`** → `config/maker/maker_<anaName>.yaml` (φ builder + femto species/channels in one file).
- **Unified phi femto (p, d, t, ³He, ⁴He):** `./script/singularity_run_anaFemtoPhi.sh` — one pass, 21 channels; mainconf `config/mainconf/main_auau3p85fxt_anaFemtoPhi.yaml`.
- **Unified K⁻ femto (p, d, t, ³He, ⁴He):** `./script/singularity_run_anaFemtoKaon.sh` — one pass, 5 channels (`kaon_minus_*`); mainconf `config/mainconf/main_auau3p85fxt_anaFemtoKaon.yaml`. CF from merged SE/ME via `singularity_checkHistAnaFemtoKaon.sh` (no sidebands).
- **Phi-4He femto:** `./script/singularity_run_anaFemtoPhi4He.sh` — same pattern as phi-p; species `he4`, channels `phi_he4_*`.
- **Phi-deuteron femto:** `./script/singularity_run_anaFemtoPhiDeuteron.sh` — same pattern; species `deuteron`, channels `phi_deuteron_*`.
- **Phi QA:** `./script/singularity_checkHistAnaPhi.sh <root_file> <mainconf_path>` — same role as `checkHistAnaPhi.sh`.
- **Phi-p femto QA:** `./script/singularity_checkHistAnaFemtoPhiProton.sh <root_file> <mainconf_path>`. Writes QA PDF plus `{anaName}_checkHistAnaFemtoPhiProton_CF_{jobid}.pdf` (15 centrality slices: SE/ME k*, raw CF, sideband-subtracted CF). Maker YAML (`FemtoConfig`): `cfCentSlices`, `cfCentSlicesQaPdfInclude`, `cfPdfExcludeQaSlices`, `sidebandSubtractAlpha`, `negativeBinPolicy`. Mixing YAML: `mixingMode` (`randomSample`|`bufferAll`), `maxMixedPairsPerEvent`, `mixBothDirections`, `bufferSize`. See `StMaker/StFemtoMaker/README.md` and `analysisnote/YYYYMMDD/femto_cent_sb_cf_plan.md`.
- **Unified phi femto QA:** `./script/singularity_checkHistAnaFemtoPhi.sh <root_file> <mainconf_path>` — single QA PDF only (representative cent slices × all channel bases; no `*_CF_*.pdf`).
- **Unified K⁻ femto QA:** `./script/singularity_checkHistAnaFemtoKaon.sh <root_file> <mainconf_path>` — K⁻ + bachelor QA, k* SE/ME, inclusive CF and cent slices in one PDF.
- **Phi-4He / phi-deuteron femto QA:** `./script/singularity_checkHistAnaFemtoPhi4He.sh` or `./script/singularity_checkHistAnaFemtoPhiDeuteron.sh` — same CF/QA PDF layout as phi-p femto (Pages 1–20 + separate CF PDF).

Example (build):

```bash
./script/singularity_make.sh config/mainconf/main_auau3p85fxt_anaPhi.yaml
```

Example (Lambda, first 100 events):

```bash
./script/singularity_run_anaLambda.sh config/picoDstList/auau19GeV_lambda.list rootfile/auau19_anaLambda_temp/out.root 0 100
```

Example (Phi smoke test):

```bash
./script/singularity_run_anaPhi.sh config/mainconf/main_auau3p85fxt_anaPhi_test.yaml "" "" 0 100
```

The wrappers source `setup.sh` (or resolve STAR from analysis_info when `starver` is unavailable), bind `/gpfs`, `/cvmfs`, and `/star/nfs4/AFS`, and for run wrappers also `/home/starlib` and site-local `/star/data*` paths from the input list when needed. Build and run wrappers then invoke `make` or the same `analysis/run_anaXxx.C` entry points as the non-singularity scripts.

### Result QA (Phi): checkHistAnaPhi.sh

After running the Phi analysis (locally or after merging batch output), you can produce a histogram QA PDF from the output ROOT file:

```bash
./script/checkHistAnaPhi.sh <root_file> <mainconf_path>
```

Example (single or merged ROOT file):

```bash
./script/checkHistAnaPhi.sh rootfile/auau3p85fxt_anaPhi/auau3p85fxt_anaPhi_CCBCC32EA67793F5A24B5F6BA44EE413_merge.root config/mainconf/main_auau3p85fxt_anaPhi.yaml
```

On **AL9** login nodes (SL7→AL9 transition), prefer the Singularity wrapper (same batch-like STAR runtime):

```bash
./script/singularity_checkHistAnaPhi.sh <root_file> <mainconf_path>
```

On **SL7**, the host script above is sufficient. Example (Singularity):

```bash
./script/singularity_checkHistAnaPhi.sh /direct/star+u/oura/rootfile/auau3p85fxt_anaPhi/auau3p85fxt_anaPhi_CCBCC32EA67793F5A24B5F6BA44EE413_merge.root config/mainconf/main_auau3p85fxt_anaPhi.yaml
```

If the input filename has the form `anaName_jobid_merge.root` (32‑char hex jobid), the PDF is written as `share/figure/<anaName>/<anaName>_checkHistAnaPhi_<jobid>.pdf`; otherwise as `share/figure/<anaName>/<anaName>_checkHistAnaPhi.pdf`. When config is loaded, cut regions (event, track, phi) are overlaid on pre-cut histograms as red dashed lines. Page **1b** shows centrality QA histograms (`hCentrality`, pileup 2D plots, etc.) when present in the ROOT file.

### Result QA (Lambda): checkHistAnaLambda.sh

After running the Lambda analysis (locally or after merging batch output), produce a histogram QA PDF:

```bash
./script/checkHistAnaLambda.sh <root_file> <mainconf_path>
```

On **AL9**, use **`./script/singularity_checkHistAnaLambda.sh`** with the same arguments (recommended during the SL7→AL9 transition). PDF naming: `share/figure/<anaName>/<anaName>_checkHistAnaLambda[_<jobid>].pdf`. Pages **1b–1d** show centrality QA when `centrality:` is enabled in mainconf and the ROOT file contains the corresponding histograms. Page **1d** shows `hLambda_InvMass_CentBin0`–`8`.

### Result QA (Phi-p femto): checkHistAnaFemtoPhiProton.sh

After merging batch output for `auau3p85fxt_anaFemtoPhiProton` (or any StFemtoMaker analysis using the same checkHist macro):

```bash
./script/singularity_checkHistAnaFemtoPhiProton.sh <merge.root> <mainconf_path>
```

Example:

```bash
./script/singularity_checkHistAnaFemtoPhiProton.sh \
  rootfile/auau3p85fxt_anaFemtoPhiProton/auau3p85fxt_anaFemtoPhiProton_FC1269B1A73EA32BC18AC1A389355EC3_merge.root \
  config/mainconf/main_auau3p85fxt_anaFemtoPhiProton.yaml
```

Writes **two** PDFs under `share/figure/<anaName>/`: QA (`..._checkHistAnaFemtoPhiProton_<jobid>.pdf`) and multi-centrality CF (`..._checkHistAnaFemtoPhiProton_CF_<jobid>.pdf`). CF is computed from merged SE/ME in the macro (not stored in ROOT). Plan record: `analysisnote/YYYYMMDD/femto_cent_sb_cf_plan.md`.

### Result QA (Phi-deuteron femto): checkHistAnaFemtoPhiDeuteron.sh

After merging batch output for `auau3p85fxt_anaFemtoPhiDeuteron`:

```bash
./script/singularity_checkHistAnaFemtoPhiDeuteron.sh <merge.root> <mainconf_path>
```

Example:

```bash
./script/singularity_checkHistAnaFemtoPhiDeuteron.sh \
  rootfile/auau3p85fxt_anaFemtoPhiDeuteron/auau3p85fxt_anaFemtoPhiDeuteron_<jobid>_merge.root \
  config/mainconf/main_auau3p85fxt_anaFemtoPhiDeuteron.yaml
```

Initial validation joblist (10 files): `job/joblist/joblist_auau3p85fxt_anaFemtoPhiDeuteron_test10.xml`.

### StRoot vendoring

- **When to vendor:** prefer `$STAR/lib` and `StMaker/` first; copy into `StRoot/<Package>/` only when STAR or past-analysis source must ship with the repo. Agent procedure: `docs/ai/skills/reuse-star-stroot.md`.
- **Template:** `StRoot/StRefMultCorr/` — `PROVENANCE.md`, dedicated `Makefile` target, load `lib/libStRefMultCorr.so` from repo `lib/` in `run_ana*.C` (never duplicate-load from `$STAR/lib`).

### Centrality (StRefMultCorr)

- **Canonical reference:** [`StRoot/StRefMultCorr/README.md`](../StRoot/StRefMultCorr/README.md) — cent9/cent16 bin table, 0–60% ↔ cent9 2–8 mapping, `CentralityHelper` event order, YAML keys, femto `cfCent9Min`/`Max`. Agent skill: `docs/ai/skills/centrality-strefmultcorr.md`.
- **Vendored library:** `StRoot/StRefMultCorr/` → `lib/libStRefMultCorr.so` (see `PROVENANCE.md`). Loaded by `run_ana*.C` before `libStCommon.so` and the analysis Maker `.so` files.
- **mainconf key:** `centrality: cuts/centrality/centrality_<anaName>.yaml` — `mode` is `refmult` (19 GeV collider) or `fxtmult` (FXT).
- **Bin index (`cent9`):** **larger index = more central** (cent9 0 = 70–80%, cent9 8 = 0–5%). Do not confuse bin **0** with “0–5% central”. Makers store `getCentralityBin9()` without remapping.
- **`acceptedCentBins`:** comma-separated **cent9 indices** (e.g. most central only: `8` or `7,8`). Empty = all 0–8.
- **`cent9MaxRefMultCorrBin` / `cent9MaxRefMultCorr`:** optional tail trim in one cent9 bin (Maker only; re-run needed in ROOT).
- **Integrated QA:** centrality histograms are filled in `StPhiMaker` / `StLambdaMaker` and appear in the Phi / Lambda QA PDFs via `checkHistAnaPhi.sh` / `checkHistAnaLambda.sh` (Pages **1b–1d**; Page **1c** expects **cent9 vs multiplicity to increase** left to right). See `analysisnote/20260521/centrality_qa_histograms.md`.
- **Standalone QA (picoDst only):** `./script/checkCentrality.sh <picoDst_or_list> <mainconf_path> [output.root] [maxEvents]` — reads `centrality:` from mainconf (`refmult` / `fxtmult`). On fragile login nodes: `./script/singularity_checkCentrality.sh` with the same arguments.

### Batch (star-submit)

First-time flow (after git clone): customize analysis info → setup → build → generate joblist → submit.

1. **Set output paths in analysis info if needed**  
   Edit **config/analysis/analysis_info_temp.yaml** (or the file your mainconf's `analysis:` points to) if you want ROOT outputs, stdout, or stderr written somewhere specific. **analysis.workDir** is the ROOT output base for the generated joblist. Optional **analysis.logDir** and **analysis.errDir** move stdout/stderr away from `workDir`; if unset, they default to `workDir/log` and `workDir/err`. Batch runtime no longer depends on the repository living at `workDir`. If `workDir` is left unset or at the template placeholder, joblist generation falls back to the current project root.

2. **Setup and build** (from project root):
   ```bash
   source ./script/setup.sh config/mainconf/main_auau19_anaLambda.yaml
   make
   ```
   Or use **`./script/singularity_make.sh config/mainconf/main_auau19_anaLambda.yaml`** for the same batch-like STAR toolchain without interactive `sl7`.
   For `csh` / `tcsh`, use `source ./script/setup.csh ...` instead. The setup script must be sourced so the selected STAR and ROOT environment persist for `make`.

3. **Generate the joblist** (from project root). Pass the mainconf path:
   ```bash
   ./script/generate_joblist.sh config/mainconf/main_auau19_anaLambda.yaml
   ```
   For another analysis (e.g. Phi):
   ```bash
   ./script/generate_joblist.sh config/mainconf/main_auau19_anaPhi.yaml
   ```
   This writes **job/joblist/joblist_<anaName>.xml** (from **job/joblist/job_template_from_conf.xml** and the analysis info; `anaName` is `analysis.anaName`). Requires the **Python 3 environment** (see above).

   For a short farm smoke test, create a dedicated `analysis_info` / `mainconf` pair (for example `auau19_anaPhi_test`) and set:
   - `analysis.nFiles: 1` to submit one input file
   - `analysis.maxEvents: 100` to run only the first 100 events inside batch `root4star`

4. **Submit** from `job/run/`:
   ```bash
   cd job/run
   ./submit.sh ../joblist/joblist_auau19_anaLambda_temp.xml
   ```
  If your generated XML uses `__PROJECT_ROOT__`, `submit.sh` will replace it with the actual project path. ROOT output URLs come from **analysis.workDir**; stdout/stderr URLs come from optional **analysis.logDir** / **analysis.errDir** or fall back to `workDir/log` and `workDir/err`. The batch runtime itself now runs from a scratch-local copy of `analysis/`, `config/`, `lib/`, `include/`, and `StMaker/`, so the repository can be moved without breaking `root4star` macro resolution.

   `submit.sh` now runs preflight before `star-submit`:
   - resolves `anaName` from `joblist_<anaName>.xml`
   - extracts the embedded `config/mainconf/...yaml` path from the joblist and treats that as the single source of truth
   - checks `python script/analysis_info_helper.py --library-tag --mainconf ...` resolves
   - checks ELF class consistency between `root4star` and key libraries under `lib/`
   - checks runtime linker resolution in singularity context (fails fast if `ldd` reports `not found`)
   - creates missing stdout, stderr, and `rootfile/<scratchSubdir>/` output directories from `<stdout>`, `<stderr>`, and `<output toURL>` and verifies they are writable

   If preflight fails with ABI mismatch hints (`wrong ELF class` class of issue), submit is stopped before scheduling. You can request one recovery rebuild attempt:
   ```bash
   ./submit.sh --rebuild-if-needed ../joblist/joblist_auau19_anaLambda_temp.xml
   ```
   This runs `source ./script/setup.sh <embedded-mainconf> && make`, then retries preflight once using the same embedded mainconf from the joblist.

   **Auto merge after all subjobs finish** (optional; QA PDF still manual):

   ```bash
   ./submit.sh --watch-merge ../joblist/joblist_<anaName>.xml
   ```

   This starts `script/watch_job_and_merge.sh` in the background. It polls until subjob ROOT file count matches the SUMS `.list` count, then runs `merge_root_files.csh`. Log: `job/run/watchmerge/watchmerge_<anaName>_<jobid>.log`. Completion is recorded in runmeta `postProcess.watchMerge`. Use `--watch-merge-foreground` to block until merge finishes. Environment: `WATCH_MERGE_POLL_SEC` (default 300), `WATCH_MERGE_TIMEOUT_SEC` (default 259200). Re-run manually: `./script/watch_job_and_merge.sh --runmeta job/run/runmeta/runmeta_<anaName>_<jobid>.json`.

   For bad subjob ROOT exclusion, pass a prepared exclusion list:
   ```bash
   ./script/watch_job_and_merge.sh \
     --runmeta job/run/runmeta/runmeta_<anaName>_<jobid>.json \
     --force-merge \
     --exclude-bad-roots /tmp/bad_subjob_roots_<anaName>_<jobid>.txt
   ```

   `merge_root_files.csh` supports:
   - `--exclude-list=<file>`: merge after excluding listed subjob ROOT paths
   - `--skip-bad-scan`: disable automatic scan

   `scan_bad_subjob_roots.sh` builds exclusion lists from a sample subjob output:
   ```bash
   ./script/scan_bad_subjob_roots.sh \
     --sample rootfile/<anaName>/<anaName>_<jobid>_0.root \
     --output /tmp/bad_subjob_roots_<anaName>_<jobid>.txt
   ```

  On successful submit, `submit.sh` now records a per-`jobid` reproducibility set:
  - **job/run/configlog/config_<anaName>_<jobid>.txt** — embedded mainconf plus all referenced config YAMLs
  - **job/run/joblistlog/joblist_<anaName>_<jobid>.xml** — submitted XML after placeholder replacement
  - **job/run/runmeta/runmeta_<anaName>_<jobid>.json** — manifest linking the saved submit-time artifacts
  - **job/run/runmeta/gitstatus_<anaName>_<jobid>.txt**, **gitdiff_<anaName>_<jobid>.patch**, and **gitsubmodules_<anaName>_<jobid>.txt** — git provenance at submit time
  - **job/run/runmeta/runtime_bundle_<anaName>_<jobid>.tar.gz** — replay bundle capturing submit-time code/runtime state
  - **job/run/runmeta/sums_artifacts_<anaName>_<jobid>.tar.gz** — stable snapshot of SUMS-generated `.list/.csh/.condor/.report/.session.xml` files for that submit
  - **job/run/runmeta/submit_stdout_<anaName>_<jobid>.txt** — captured `star-submit` output

  This means a `jobid` now resolves not only to config and XML, but also to the submit-time code state and exact SUMS-expanded job wrappers. See [job/run/README.md](../job/run/README.md) for the operational view.

5. **Cleaning up job/run**  
   After submission, `job/run/` is filled with many files named `anaName+jobid+*` (`.csh`, `.list`, etc.). The stable submit-time copy now lives in `job/run/runmeta/sums_artifacts_<anaName>_<jobid>.tar.gz`, so removing or archiving the loose originals is no longer your only reproducibility path. To delete them (avoids "Argument list too long"): `cd job/run && ./cleanup_job_run.sh <anaName+jobid>`. To move them into an archive instead: `cd job/run && ./archive_job_run.sh <anaName+jobid>` (files go to `job/run/joblog/<anaName>/`; the directory is created if needed).

## Adding a new analysis (new StMaker)

Each analysis has:

1. A Maker in `StMaker/StXXXMaker/` (e.g. `StLambdaMaker`).
2. A shared library `lib/libStXXXMaker.so` (e.g. `libStLambdaMaker.so`).
3. Two macros: `analysis/run_anaXxx.C` and `analysis/anaXxx.C`.
4. A run script: `script/run_anaXxx.sh`.

### 1. Maker code

- Create `StMaker/StXXXMaker/StXXXMaker.h` and `StMaker/StXXXMaker/StXXXMaker.cxx`.
- Subclass `StMaker`; implement `Init()`, `Make()`, `Clear()`, `Finish()`.
- Create histograms in `DeclareHistograms()` and write them in `WriteHistograms()` (e.g. in `Finish()`).
- Use `ConfigManager::GetInstance()` for cuts and `GetHistConfigPath()` for the hist YAML path if your Maker uses config.
- Use `StPicoDstMaker` to access PicoDst; get the chain from it as needed.

### 2. Build the shared library

- Place sources as `StMaker/StXXXMaker/StXXXMaker.h` and `StMaker/StXXXMaker/StXXXMaker.cxx`. The Makefile auto-discovers `StMaker/St*Maker/` and builds `lib/libStXXXMaker.so` (no Makefile edit). Shared helpers belong in `StMaker/common/` and link via `lib/libStCommon.so`.
- Run `make` so that `lib/libStXXXMaker.so` (and `lib/libStCommon.so` if common sources changed) exists.

### 3. Runner macro (run_anaXxx.C)

- Copy `analysis/run_anaPhi.C` (or `run_anaLambda.C`) to `analysis/run_anaXxx.C`.
- Replace the analysis name in: function name, library `libStXXXMaker.so`, macro `anaXxx.C+`, and call `anaXxx(...)`. Load order: `libStarAnaConfig.so` → `libStRefMultCorr.so` → `libStCommon.so` → Maker `.so`. Keep `-lStCommon` in `AddLinkedLibs`, and pass the config path as the 5th argument where applicable.

### 4. Analysis macro (anaXxx.C)

- Copy `analysis/anaPhi.C` or `analysis/anaLambda.C` to `analysis/anaXxx.C`.
- Before building the chain, load the main config: `ConfigManager::GetInstance().LoadConfig(mainConfigPath)` (resolve `mainConfigPath` from the 5th argument or default to e.g. `config/mainconf/main_XXX.yaml`).
- Replace the Maker type and variable names; keep the same signature so the runner can call it (including optional 5th parameter `configPath` if used).

### 5. Run script (run_anaXxx.sh)

- Copy `script/run_anaPhi.sh` or `script/run_anaLambda.sh` to `script/run_anaXxx.sh`.
- Point to `run_anaXxx.C` and set default input, output, and config paths.

## Creating your own config

- **Main config**: Copy `config/mainconf/main_auau19_anaLambda.yaml` (or `main_auau19_anaPhi.yaml`) to e.g. `config/mainconf/main_myanalysis.yaml`. It references (paths are relative to `config/`):
  - **Cuts**: `event`, `track`, `pid`, `v0`, `mixing`, and optionally analysis-specific keys (e.g. `phi`, `lambda`).
  - **Maker**: e.g. `lambda: maker/maker_lambda.yaml` (StPhiMaker / StLambdaMaker), or `maker: maker/maker_<anaName>.yaml` (StFemtoMaker).
  - **Hist**: `hist: hist/hist_lambda.yaml`.
  - **Analysis info**: `analysis: analysis/analysis_info_temp.yaml` (or your own file). This file is used by `setup.sh` and by `script/analysis_info_helper.py --generate-joblist`.
- **Maker config**: Add e.g. `config/maker/maker_my.yaml` and reference it in the main config under the key your Maker expects. Makers read cuts via `ConfigManager::GetInstance().GetXXXCuts()` and the hist path via `GetHistConfigPath()`.

### Phi pair rapidity frame (lab / CM)

`StPhiMaker` fills pair-rapidity histograms and applies `minPairRapidity` / `maxPairRapidity` in the **analysis frame** configured in the phi maker YAML:

| Key | Values | Meaning |
|-----|--------|---------|
| `rapidityFrame` | `auto`, `lab`, `cm` | `auto`: CM if `centrality.mode` is `fxtmult`, else lab |
| `sqrtSNNGeV` | GeV | \(\sqrt{s_{NN}}\) for automatic CM shift (fixed target) |
| `rapidityShift` | float | Manual \(y_{\mathrm{shift}}\); if set, skips auto calculation |
| `nucleonMassGeV` | GeV/c² | Nucleon mass for auto shift (default `0.938272`) |

**Auto shift (FXT):** for a target nucleon at rest, \(E_{\mathrm{beam}}=(s-2m^2)/(2m)\), \(\beta_{\mathrm{cm}}=p_{\mathrm{beam}}/(E_{\mathrm{beam}}+m)\), \(y_{\mathrm{shift}}=\mathrm{atanh}(\beta_{\mathrm{cm}})\), and **\(y_{\mathrm{ana}} = y_{\mathrm{lab}} - y_{\mathrm{shift}}\)**.

**Examples:**

- Collider (`centrality.mode: refmult`): `rapidityFrame: auto` → lab, no shift.
- FXT (`centrality.mode: fxtmult`): `rapidityFrame: auto`, `sqrtSNNGeV: 3.85` → CM shift computed at `StPhiMaker::Init()`.

`checkHistAnaPhi` PDF header notes the resolved frame when config loads. Pair-rapidity **cuts** in YAML are in the same frame as the histograms; after enabling CM for FXT, retune lab-window cuts (e.g. `[-1.5,-1.0]`) to a CM-centered window if needed.

- **Hist config**: Add e.g. `config/hist/hist_my.yaml` with the same structure as existing hist YAMLs (`axes`, `histograms`). Set the `hist` key in the main config to this file.
- **New cut type**: If you need a new cut category, add a new key in the main YAML, a new `XxxCutConfig` in `include/cuts/` and `src/cuts/`, and register it in `ConfigManager`. For a new analysis that only uses existing event/track/pid/v0/mixing and maker keys, copying and editing the existing YAMLs under `config/cuts/`, `config/maker/`, and `config/hist/` is enough.

## Creating a joblist

**Recommended: generate from analysis info**

1. Ensure **config/analysis/analysis_info_temp.yaml** (or the file referenced by your mainconf's `analysis:` key) has the keys described in **Analysis info** above, including `anaName`, optional `workDir` for ROOT output, optional `logDir` / `errDir` for stdout/stderr, `baseRunMacro`, `baseAnaMacro`, `jobName`, `scratchSubdir`, `outputFileStem`, `nFiles`, optional `maxEvents`, and the `starTag` fields for the catalog URL.
2. From the project root, run (requires the **Python 3 environment**):
   ```bash
   ./script/generate_joblist.sh config/mainconf/main_myanalysis.yaml
   ```
   This fills **job/joblist/job_template_from_conf.xml** from the analysis info and writes **job/joblist/joblist_<anaName>.xml**. The batch command inside the XML receives the same mainconf path you passed to `generate_joblist.sh`, and if `analysis.maxEvents` is set it is forwarded to batch `root4star`.
3. Submit from `job/run`: `./submit.sh ../joblist/joblist_<anaName>.xml`. Preflight uses the mainconf embedded in the joblist, not a basename-derived `main_<anaName>.yaml`.

**Manual edit**

- **Templates** in `job/joblist/`: **job_template_from_conf.xml** (for script-based generation) and concrete joblists (e.g. `joblist_auau19_anaLambda_test.xml`).
- To edit by hand: copy a joblist to a new name, then set **command** (macro, `$FILELIST`, `$SCRATCH` path, config path), **stdout/stderr/output toURL** (e.g. from your `workDir`), **input** (catalog URL, `nFiles`), and **SandBox** `File` entries as needed.
- Submit from `job/run`: `./submit.sh ../joblist/YourJoblist.xml`. Ensure `make` has been run so `lib/` contains the required `.so` files.
- In batch jobs, command preamble should clear ACLiC outputs with the analysis prefix (now `analysis/<baseAnaMacro>_C.*`) to avoid stale `.so/.d/.pcm` contamination between different build environments.
- For `auau19_anaLambda` batch jobs, joblists use `singularity exec` + `-B /star/nfs4/AFS` + `-B /home/starlib:/home/starlib` (optional if inputs are staged) and preserve `LD_LIBRARY_PATH` to avoid `libgfortran.so.3` loader failures.
- Spack-style `root4star` in this workflow does not ship `Netx`/`RFIO` plugins, so `root://` / `rfio://` paths in a SUMS `.list` fail inside the container unless rewritten. The test joblist runs `sed` on `$FILELIST` to strip those schemes to POSIX paths; with `copyInputLocally="true"`, SUMS stages via `xrdcp` into `$SCRATCH/INPUTFILES` and passes a `.local.list` (plain paths) — that is the reliable mode for full chains. Do not wrap production `root4star` in a short `timeout` (debug-only); it can kill the job before output is written.
- To reduce inaccessible input entries, the current catalog filter excludes ADC files (`filename!~adc`) in the test/debug joblists.
