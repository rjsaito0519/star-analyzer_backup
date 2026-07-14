# star-submit run directory

Run `star-submit` from **this directory**. SUMS will write generated files (`.csh`, `.list`, `.condor`, `.report`, etc.) here.

## Steps

1. **Build at the project root** (batch-matched STAR toolchain — **either** path is valid)
   ```bash
   cd /path/to/star-analysis
   ./script/singularity_make.sh config/mainconf/main_auau19_anaLambda.yaml
   ```
   **Or** interactive SL7:
   ```bash
   cd /path/to/star-analysis
   sl7
   source ./script/setup.sh config/mainconf/main_auau19_anaLambda.yaml
   make
   ```
   `singularity_make.sh` runs `make` inside `star-bnl/star-sw:latest` with the same `sl73_*` / `sl74_*` toolchain as farm jobs; use it when you do not want to enter `sl7` manually (for example on AL9 login nodes). For `csh` / `tcsh`, use `source ./script/setup.csh ...` instead when building inside `sl7`.
   For debug/repro jobs (especially heap/exit issues), always use one of these batch-like builds before submit — not host-only `make` on a mismatched OS.

2. **Move to this directory and submit**
   ```bash
   cd job/run
   ./submit.sh ../joblist/joblist_auau19_anaLambda_test.xml
   ```
   Pass the joblist XML you want (for example the file produced by `./script/generate_joblist.sh`, named **`joblist_<anaName>.xml`** where **`anaName`** is `analysis.anaName` in your analysis_info). By default, `submit.sh` uses **`../joblist/joblist_auau19_anaLambda_temp.xml`** if you omit the argument (adjust the default in `submit.sh` if your primary analysis differs). Example for Phi: `./submit.sh ../joblist/joblist_auau19_anaPhi.xml`.

  `submit.sh` replaces `__PROJECT_ROOT__` in the template with the actual project path, so the same template works for any user.
  Before submit it runs a preflight check: it infers **`anaName`** from the joblist basename (`joblist_<anaName>.xml` → `<anaName>`), then extracts the embedded **`config/mainconf/...yaml`** path from the joblist and uses that as the single source of truth for `libraryTag` resolution, rebuilds, ELF class consistency of `lib/*.so` vs `root4star`, and runtime linker sanity in the singularity context. This fails fast on mismatch and avoids secondary errors like missing `fromScratch` output after an early crash.

  After preflight succeeds, **`submit.sh` creates the stdout, stderr, and ROOT output directories parsed from `<stdout>`, `<stderr>`, and `<output toURL>`** if they are missing, and verifies each directory is writable. Generated joblists use `analysis.workDir` for ROOT output and optional `analysis.logDir` / `analysis.errDir` for stdout/stderr. This prevents SUMS from scheduling a job that later fails when copying output.

   If preflight fails and you want the script to try recovery once, use:
   ```bash
   ./submit.sh --rebuild-if-needed ../joblist/joblist_auau19_anaLambda_test.xml
   ```
   This runs `source ./script/setup.sh <embedded-mainconf> && make`, then retries preflight with the same embedded mainconf. If host `make` is not batch-matched (for example AL9), run **`./script/singularity_make.sh <embedded-mainconf>`** at the project root first, then re-run `submit.sh` without relying on this recovery path.

  On successful submit, reproducibility artifacts are saved per `jobid`:
  - **joblistlog/joblist_<anaName>_<jobid>.xml** — submitted XML after `__PROJECT_ROOT__` replacement
  - **configlog/config_<anaName>_<jobid>.txt** — embedded mainconf plus all referenced YAMLs
  - **runmeta/runmeta_<anaName>_<jobid>.json** — machine-readable manifest tying together the saved artifacts below
  - **runmeta/gitstatus_<anaName>_<jobid>.txt** — `git status --porcelain=v2 --branch` at submit time
  - **runmeta/gitdiff_<anaName>_<jobid>.patch** — `git diff --binary HEAD` at submit time
  - **runmeta/gitsubmodules_<anaName>_<jobid>.txt** — `git submodule status --recursive` at submit time
  - **runmeta/runtime_bundle_<anaName>_<jobid>.tar.gz** — submit-time code/runtime bundle for replay (`analysis/`, `config/`, `include/`, `StMaker/`, `StRoot/`, `lib/`, and build/source metadata when present)
  - **runmeta/sums_artifacts_<anaName>_<jobid>.tar.gz** — stable snapshot of SUMS-generated `anaName+jobid+*` files such as `.list`, `.csh`, `.condor`, `.report`, and `.session.xml`
  - **runmeta/submit_stdout_<anaName>_<jobid>.txt** — captured `star-submit` console output

## Auto merge after batch completion (`--watch-merge`)

To poll subjob ROOT output and run `merge_root_files.csh` automatically when all subjobs finish:

```bash
cd job/run
./submit.sh --watch-merge ../joblist/joblist_auau3p85fxt_anaFemtoPhiProton.xml
```

Unified phi femto (all bachelors in one pass):

```bash
./submit.sh --watch-merge ../joblist/joblist_auau3p85fxt_anaFemtoPhi.xml
```

After merge, QA PDF:

```bash
./script/singularity_checkHistAnaFemtoPhi.sh \
  rootfile/auau3p85fxt_anaFemtoPhi/auau3p85fxt_anaFemtoPhi_<jobid>_merge.root \
  config/mainconf/main_auau3p85fxt_anaFemtoPhi.yaml
```

Unified K⁻ femto (all bachelors in one pass):

```bash
./submit.sh --watch-merge ../joblist/joblist_auau3p85fxt_anaFemtoKaon.xml
```

After merge, QA + CF PDF:

```bash
./script/singularity_checkHistAnaFemtoKaon.sh \
  rootfile/auau3p85fxt_anaFemtoKaon/auau3p85fxt_anaFemtoKaon_<jobid>_merge.root \
  config/mainconf/main_auau3p85fxt_anaFemtoKaon.yaml
```

- **`--watch-merge`**: after a successful submit, starts `script/watch_job_and_merge.sh` in the background (`nohup`).
- **`--watch-merge-foreground`**: same watcher, but blocks until merge finishes (debug).
- **Log**: `job/run/watchmerge/watchmerge_<anaName>_<jobid>.log`
- **Completion rule**: count subjob ROOT files under `rootfile/<anaName>/` (excluding `*_merge.root`) until it matches the number of SUMS `{anaName}{jobid}_*.list` files in `job/run/`; requires two consecutive polls with the same count (GPFS settle). Default poll interval: 300 s (`WATCH_MERGE_POLL_SEC`). Default timeout: 72 h (`WATCH_MERGE_TIMEOUT_SEC` or `--timeout-hours` on the watch script).
- **runmeta update**: on finish, `runmeta_<anaName>_<jobid>.json` gets a `postProcess.watchMerge` block (`status`, paths, counts).
- **Skip**: if `*_merge.root` already exists, the watcher exits without re-merging (use `watch_job_and_merge.sh --force-merge` to override).
- **Bad-root exclusion (manual list)**: `watch_job_and_merge.sh --exclude-bad-roots <list.txt>` forwards to `merge_root_files.csh --exclude-list=<list.txt>`.
- **QA PDF**: not run automatically; after merge, run the usual `singularity_checkHistAna*` script on `*_merge.root` manually.

Manual re-run (e.g. after a timeout):

```bash
./script/watch_job_and_merge.sh --runmeta job/run/runmeta/runmeta_<anaName>_<jobid>.json
```

Manual re-run with exclusion list:

```bash
./script/watch_job_and_merge.sh \
  --runmeta job/run/runmeta/runmeta_<anaName>_<jobid>.json \
  --force-merge \
  --exclude-bad-roots /tmp/bad_subjob_roots_<anaName>_<jobid>.txt
```

## Manual bad-root scan and re-merge

Use this when `*_merge.root` looks suspiciously small or `hadd` failed due to bad subjob ROOT files.

```bash
# 1) Build exclusion list from a sample subjob file
./script/scan_bad_subjob_roots.sh \
  --sample rootfile/<anaName>/<anaName>_<jobid>_0.root \
  --output /tmp/bad_subjob_roots_<anaName>_<jobid>.txt

# 2) Re-merge with exclusions (auto-overwrites *_merge.root)
./script/merge_root_files.csh \
  --exclude-list=/tmp/bad_subjob_roots_<anaName>_<jobid>.txt \
  rootfile/<anaName>/<anaName>_<jobid>_0.root
```

Notes:
- `merge_root_files.csh` now runs `scan_bad_subjob_roots.sh` automatically by default.
- Use `--skip-bad-scan` to disable automatic scan.
- Use `--exclude-list=<file>` to force a specific exclusion list.

## Quick Check

- Generate from the intended mainconf: `./script/generate_joblist.sh config/mainconf/main_auau19_anaPhi_test.yaml`
- Confirm the joblist embeds the same path: `python script/analysis_info_helper.py --mainconf-from-joblist job/joblist/joblist_auau19_anaPhi_test.xml`
- Confirm setup metadata resolves from that same path: `python script/analysis_info_helper.py --library-tag --mainconf config/mainconf/main_auau19_anaPhi_test.yaml`
- For short farm smoke tests, set `analysis.maxEvents` (for example `100`) in the referenced `analysis_info`; the generated batch command will pass that as the 4th `root4star` argument
- Submit and check the preflight log prints the same mainconf path before `star-submit`
- After submit, verify `runmeta/runmeta_<anaName>_<jobid>.json` exists and points to the expected sidecars

## Cleaning up this directory

After submission, SUMS leaves many files named `anaName+jobid+*` (e.g. `auau3p85fxt_anaPhiCCBCC32EA67793F5A24B5F6BA44EE413_0.csh`). To avoid "Argument list too long" when removing them:

- **Delete** all matching files: `./cleanup_job_run.sh <anaName+jobid>` (e.g. `./cleanup_job_run.sh auau3p85fxt_anaPhiCCBCC32EA67793F5A24B5F6BA44EE413`).
- **Archive** them under `joblog/<anaName>/`: `./archive_job_run.sh <anaName+jobid>` (creates `joblog/<anaName>/` if needed).

`runmeta/sums_artifacts_<anaName>_<jobid>.tar.gz` already preserves a stable submit-time copy of those SUMS-generated files, so cleanup/archive of the loose originals is now an operational convenience rather than the only reproducibility path.

## Notes

- Always `cd` into **job/run/** before running `./submit.sh`.
- To use a different joblist template, run e.g. `./submit.sh ../joblist/YourJoblist.xml`.
- Submit-time reproducibility artifacts now live in `job/run/runmeta/`; `script/archive_all_job_logs.sh` archives that directory alongside `configlog` and `joblistlog`.
- Batch command now clears `analysis/<baseAnaMacro>_C.*` before `root4star` so stale ACLiC outputs (`.so/.d/.pcm` etc.) do not mix across environments.
- Batch runtime now copies `analysis/`, `config/`, `lib/`, `include/`, `StMaker/`, `StRoot/`, and optional build artifacts into a scratch-local runtime bundle before `singularity exec`, so moved or symlinked repositories do not break macro lookup inside the container.
- Current `auau19_anaLambda` joblists run `root4star` via `singularity exec ... star-bnl/star-sw:latest` with `-B /star/nfs4/AFS`, `-B /home/starlib:/home/starlib`, and inherited `LD_LIBRARY_PATH` to satisfy `libgfortran.so.3`.
- The same container strategy is available for local Phi QA via `script/singularity_checkHistAnaPhi.sh` when host `root4star` cannot start due to missing runtime libraries.
- For local LL/KP CF fitting on AL9, use `script/singularity_run_fitCorrelation.sh <root_file> <mainconf> [hist_name]` (compiled `fit_correlation` binary inside Singularity; input must be a TH1D CF histogram, not raw maker merge ROOT).
- Spack `root-config` / batch `root4star` builds here omit `Netx`/`RFIO` plugins, so `root://` and `rfio://` URLs in the SUMS `.list` cannot be opened. The test joblist rewrites the list to POSIX paths (`sed` strips `root://host:port//` and `rfio://`) before `root4star`, then reads `/home/starlib/...` as local files inside the container.

## root4star exit abort (`corrupted size vs. prev_size`)

If analysis finishes and ROOT files copy successfully but stderr shows glibc heap errors at process exit, see **`TROUBLESHOOTING_root4star_exit.md`** in this directory. It lists **debug joblists** (minimal exit, `maxEvt=1` / `100`, ACLiC vs interpreter, `gSystem->Exit(0)`), **Pr3** `Make()` stage bisect (`joblist_auau19_anaLambda_debug_pr3_max1_no_hist_s[0-4].xml`), **Pr4** Phi control and optional **`MALLOC_CHECK_=3`** joblist, a **reproduction matrix** template, **hypotheses**, and **operational success criteria** when a full fix is not available. For a **local symbolic backtrace** (no gdb in `star-sw:latest`), use **`./local_root4star_anaLambda_backtrace.sh`**. The matrix also includes **`joblist_auau19_anaLambda_debug_max1_no_hist.xml`** (Lambda without `HistManager`); see troubleshooting row **B2**.
