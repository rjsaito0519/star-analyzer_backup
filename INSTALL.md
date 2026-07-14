# Install and first run

Step-by-step: **clone → configure → build → run locally** (optional: batch). For design background see [PHILOSOPHY.md](PHILOSOPHY.md). For command-line details, tables, and extending the framework see [docs/REFERENCE.md](docs/REFERENCE.md). A short overview is in [README.md](README.md).

---

## Step 0: Prerequisites

Confirm the following **before** cloning (each line: what / why).

| Requirement | What | Why |
|-------------|------|-----|
| **STAR stack** | You can run `starver` and use `root4star`. | Makers and macros assume STAR ROOT and libraries. |
| **Build** | `gcc`, `make`, CMake; `root-config` on `PATH` after `starver`. | Builds `libStarAnaConfig.so`, `libStCommon.so`, and `libSt*Maker.so` files. |
| **Git** | Clone and submodules. | `yaml-cpp` lives in a submodule. |
| **Python 3** | `python3` available. | Used by `setup_config_from_analysisinfo.py` and optional list/joblist scripts. |
| **PyYAML** | `python3 -m pip install --user pyyaml` (if you lack it). | **Required** for `./script/generate_joblist.sh` (batch). Optional for local-only runs if you do not generate joblists. |

---

## Step 1: Clone

**What:** Get a copy of the repository. **Why:** Local tree for build and config.

```bash
git clone <repository-url>
cd <repository-directory>
```

Use your real URL and directory name instead of the placeholders above.

---

## Step 2: Submodules

**What:** Populate `src/third_party/yaml-cpp`. **Why:** Without it, `make` fails building `libStarAnaConfig.so`.

```bash
git submodule update --init --recursive
```

Or clone with: `git clone --recurse-submodules <repository-url>` and skip the command.

**Gotcha:** Forgetting this step is the most common first build failure.

---

## Step 3: Runtime directories

**What:** Create writable dirs that are git-ignored. **Why:** Batch and local jobs write logs and ROOT output here.

```bash
mkdir -p log err rootfile
```

- **log/** — default job stdout location (e.g. `log/stdout.$JOBID.out`) when `analysis.logDir` is unset.
- **err/** — default job stderr location when `analysis.errDir` is unset.
- **rootfile/** — output ROOT files (use subdirs per analysis, e.g. `rootfile/auau19_anaLambda/`).
- **lib/** — produced by `make` (no need to create).
- **share/figure/** — QA PDFs from `checkHistAnaPhi.sh` / `checkHistAnaLambda.sh` or the matching `singularity_checkHistAna*` scripts; often created on demand.

---

## Step 4: analysis_info and mainconf

**What:** Point the framework at your STAR version, paths, and analysis name. **Why:** `setup.sh` and joblists read this metadata.

**Option A — use a mainconf already in the repo (e.g. Lambda / Phi)**

1. Edit `config/analysis/analysis_info_temp.yaml` (or whatever your mainconf’s `analysis:` key references).
2. Set **analysis.workDir** only if you want a specific destination for generated ROOT outputs. If left unset or left at the template placeholder, joblist generation falls back to the current project root. Batch runtime no longer depends on the repository living at `workDir`.
3. Optionally set **analysis.logDir** and **analysis.errDir** to move batch stdout/stderr away from `workDir` (for example to `/tmp/oura/star-analyzer/log` and `/tmp/oura/star-analyzer/err`). If unset, they default to `workDir/log` and `workDir/err`.
4. Adjust **starTag.libraryTag** if your site needs a different `starver` tag.
5. Use that mainconf in Step 6 (e.g. `config/mainconf/main_auau19_anaLambda.yaml`).

**Option B — new analysis from the template**

1. Copy `config/analysis/analysis_info_temp.yaml` to `config/analysis/analysis_info_<anaName>.yaml`.
2. Edit `anaName`, `workDir` (optional ROOT output base), optional `logDir` / `errDir`, macro names, and related keys (see [docs/REFERENCE.md](docs/REFERENCE.md) — Analysis info).
3. Ensure `config/mainconf/mainconf.yaml` exists and uses the `__ANANAME__` placeholder where analysis-specific names are required.
4. Generate configs:

```bash
python script/setup_config_from_analysisinfo.py config/analysis/analysis_info_<anaName>.yaml
```

This creates `config/mainconf/main_<anaName>.yaml` and referenced cut/maker YAMLs.
**Note:** If a destination YAML file already exists, the script will skip it to prevent overwriting your customized settings. If you want to force overwrite all existing files from templates, add the `--force` option:

```bash
python script/setup_config_from_analysisinfo.py config/analysis/analysis_info_<anaName>.yaml --force
```

Replace `<anaName>` with your token (e.g. `auau19_anaLambda`).

---

## Step 5 (optional): PicoDst file list

**What:** Build an input `.list` from the STAR FileCatalog. **Why:** Local or batch runs need an input list.

```bash
python script/generate_picodst_list.py config/mainconf/main_<anaName>.yaml
```

Use `--dry-run` to print the command without running. Requires a working STAR environment. Output path comes from analysis_info (often `config/picoDstList/<anaName>.list`).

---

## Step 6: Setup and build

**What:** Run `starver` from analysis_info and compile libraries. **Why:** `make` needs `$STAR` and ROOT from the correct release.

From the **project root**, run one of:

```bash
source ./script/setup.sh config/mainconf/main_<anaName>.yaml
make
```

```csh
source ./script/setup.csh config/mainconf/main_<anaName>.yaml
make
```

**Note:** `setup.sh` / `setup.csh` must be **sourced**, not executed, because they set `STAR`, `STAR_HOST_SYS`, `PATH`, and `LD_LIBRARY_PATH` in your current shell before `make`.

**SL7-class build without an interactive `sl7` shell:** from the project root you can run **`./script/singularity_make.sh config/mainconf/main_<anaName>.yaml`**. That invokes `make` inside the same **`star-bnl/star-sw:latest`** Singularity image as batch jobs (STAR `sl73_*` / `sl74_*` toolchain), so it is the usual way to satisfy “build like SL7 / like the farm” when you are on AL9, a generic login node, or simply prefer one command over entering `sl7` manually. Default is `make clean && make` with `BUILD_BITS=64`. Use **`--no-clean`** to skip `make clean` when `src/third_party/yaml-cpp/build` is already present. See [docs/REFERENCE.md](docs/REFERENCE.md) — Local with Singularity.

---

## Step 7: Run locally

**What:** Execute one analysis interactively or in batch mode on the login node. **Why:** Validate build and config before farm submission.

Example (Lambda, first 100 events):

```bash
./script/run_anaLambda.sh config/picoDstList/auau19GeV.list rootfile/auau19_anaLambda_temp/out.root 0 100 config/mainconf/main_auau19_anaLambda.yaml
```

Specify the mainconf configuration file as the fifth argument (which is required to load the correct analysis cuts and parameters, especially for systems other than the default Au+Au 19 GeV Lambda; see [docs/REFERENCE.md](docs/REFERENCE.md) — How to run). For Phi, use `./script/run_anaPhi.sh` with its matching list and mainconf.

STAR login nodes are moving from **SL7 to AL9**. On **SL7** (or when host `root4star` works as on legacy nodes), use the plain `run_ana*` and `checkHistAna*` scripts. On **AL9**, prefer the **`singularity_*` wrappers** (same **`star-bnl/star-sw:latest`** image as batch jobs) for build, run, and QA PDFs: **`singularity_make.sh`**, **`singularity_run_anaLambda.sh`** / **`singularity_run_anaPhi.sh`**, and **`singularity_checkHistAnaLambda.sh`** / **`singularity_checkHistAnaPhi.sh`**. See [docs/REFERENCE.md](docs/REFERENCE.md) — Local with Singularity.

Example (Lambda via Singularity, first 100 events; same arguments as above):

```bash
./script/singularity_run_anaLambda.sh config/picoDstList/auau19GeV.list rootfile/auau19_anaLambda_temp/out.root 0 100 config/mainconf/main_auau19_anaLambda.yaml
```

The run scripts set **`LD_LIBRARY_PATH`** (and related env) before `root4star`. If you invoke `root4star` by hand, you must reproduce that environment or linking may fail.

For LL/KP femto CF fits, use the Singularity wrapper on **AL9** (compiles `fit_correlation` with `g++` inside the SL7-like container; no ACLiC):

```bash
./script/singularity_run_fitCorrelation.sh <root_file> <mainconf_path> [hist_name]
```

The input ROOT file must contain a **TH1D correlation function** (default name `hCF`). Batch merge output from `StFemtoMaker` does not include CF histograms; build or export CF first (e.g. from checkHist), then fit.

List keys in a ROOT file:

```bash
$(readlink -f share/femtocalc)/fit_correlation --list <root_file>
```

(rebuild via `singularity_run_fitCorrelation.sh` if the binary is missing).

---

## Optional: Histogram QA (Lambda)

**What:** Build a PDF of histograms from `run_anaLambda.sh` (or batch merge) output.  
**Why:** Quick visual check of event-level QA, centrality (Pages 1b–1d when enabled in mainconf), and Λ invariant-mass spectra.

**When:** After Step 7 (local run) or after merging batch ROOT files under `rootfile/<anaName>/`.

**SL7** — from the project root:

```bash
./script/checkHistAnaLambda.sh <root_file> <mainconf_path>
```

**AL9 (recommended during SL7→AL9 transition)** — same arguments via Singularity:

```bash
./script/singularity_checkHistAnaLambda.sh <root_file> <mainconf_path>
```

Example (19 GeV Lambda):

```bash
./script/singularity_checkHistAnaLambda.sh \
  rootfile/auau19_anaLambda/auau19_anaLambda_<jobid>.root \
  config/mainconf/main_auau19_anaLambda.yaml
```

- **PDF output:** `share/figure/<anaName>/<anaName>_checkHistAnaLambda[_<jobid>].pdf` (same jobid naming as Phi when the input is `anaName_<32hex>_merge.root`).
- **mainconf:** Must match the run that produced the ROOT file.
- **Centrality pages:** Present only if centrality is enabled (`centrality:` in mainconf) and the ROOT file contains `hCentrality`, etc.

See [docs/REFERENCE.md](docs/REFERENCE.md) — Result QA (Lambda) and Centrality.

---

## Step 8 (optional): Batch (star-submit)

**What:** Generate a joblist XML and submit. **Why:** Process many files on the farm.

Requires **Python 3 + PyYAML** (Step 0).

```bash
./script/generate_joblist.sh config/mainconf/main_<anaName>.yaml
cd job/run
./submit.sh ../joblist/joblist_<anaName>.xml
```

The generator writes **`job/joblist/joblist_<anaName>.xml`**, where **`<anaName>`** is `analysis.anaName` in your analysis_info (the same string used in `scratchSubdir` / `outputFileStem` if you use YAML aliases). The joblist embeds the exact mainconf path you passed to `generate_joblist.sh`, and **`submit.sh` preflight** uses that embedded path as its source of truth rather than deriving a mainconf from the joblist basename.

For a short batch smoke test, make a dedicated test config pair (for example `main_auau19_anaPhi_test.yaml` + `analysis_info_auau19_anaPhi_test.yaml`) and set:
- `analysis.nFiles: 1`
- `analysis.maxEvents: 100`

Then generate and submit that test mainconf in the same way:

```bash
./script/generate_joblist.sh config/mainconf/main_auau19_anaPhi_test.yaml
cd job/run
./submit.sh ../joblist/joblist_auau19_anaPhi_test.xml
```

After submit, see [job/run/README.md](job/run/README.md) for `configlog`, `cleanup_job_run.sh`, and `archive_job_run.sh`.

---

## Troubleshooting (short)

| Symptom | Things to check |
|---------|-------------------|
| `make` fails in yaml-cpp / config lib | Submodule: Step 2. After `make clean`, CMake is required to rebuild `yaml-cpp`; on hosts without a usable host `cmake`, use **`./script/singularity_make.sh <mainconf>`** or **`--no-clean`** if the yaml-cpp build tree already exists. |
| Wrong STAR / missing `root-config` | Re-source `script/setup.sh` / `script/setup.csh` (Step 6), then verify `echo $STAR`, `echo $STAR_HOST_SYS`, `which root-config`, and `root-config --cflags` before `make`. On hosts where host `make` fails, use **`script/singularity_make.sh <mainconf>`**. |
| Library load errors at runtime | On **SL7**, run via **`script/run_anaXxx.sh`** (or match its `LD_LIBRARY_PATH`). On **AL9**, use **`singularity_run_anaLambda.sh`** / **`singularity_run_anaPhi.sh`** and **`singularity_checkHistAnaLambda.sh`** / **`singularity_checkHistAnaPhi.sh`** for QA PDFs. |
| Joblist script errors | Install PyYAML for the same `python3` you use. |
| Batch paths wrong | **analysis.workDir** for ROOT output, optional **analysis.logDir** / **analysis.errDir** for stdout/stderr, plus any hand-written stdout/stderr/output paths in custom joblists (Step 4). |

---

## Quick reference (existing Lambda mainconf)

After editing `config/analysis/analysis_info_temp.yaml` (optionally **analysis.workDir** for ROOT output and **analysis.logDir** / **analysis.errDir** for batch logs):

```bash
git submodule update --init --recursive
mkdir -p log err rootfile
source ./script/setup.sh config/mainconf/main_auau19_anaLambda.yaml
make
./script/run_anaLambda.sh config/picoDstList/auau19GeV_lambda.list rootfile/auau19_anaLambda_temp/out.root 0 -1
# Optional batch:
./script/generate_joblist.sh config/mainconf/main_auau19_anaLambda.yaml
cd job/run && ./submit.sh ../joblist/joblist_auau19_anaLambda_temp.xml
```

---

## More information

- [README.md](README.md) — Overview and documentation index.
- [PHILOSOPHY.md](PHILOSOPHY.md) — Design principles and two-macro rationale.
- [docs/REFERENCE.md](docs/REFERENCE.md) — Full reference (analysis_info, batch detail, adding analyses, config, joblists, QA).
- [job/run/README.md](job/run/README.md) — Submit directory and cleanup.
- [docs/ai/INSTALL.md](docs/ai/INSTALL.md) — Setup details for AI custom skills.

---

## AI Assistant Custom Skills (Optional)

If you are using an AI assistant like **Cursor** or **Antigravity (Gemini IDE)**, you can load repository-specific custom skills (e.g. daily logs generation, analysis scaffolding, histogram instructions).

* **Cursor**: Automatically active. Custom skills are located in `.cursor/skills/`.
* **Antigravity (Gemini)**: Requires a one-time registration of the project plugin:
  ```tcsh
  tcsh script/setup_antigravity_skills.csh
  ```
  For more details, see [docs/ai/PrepareAntigravity.md](docs/ai/PrepareAntigravity.md).


