# Personal / local agent notes (NOT git-managed)

This file is the **source of truth for personal submit/merge ops** in this clone.
It is under `local/` (see `.git/info/exclude`). Do **not** fold these conventions into
shared `docs/ai/*`, `PHILOSOPHY.md`, `.gitignore`, or official `job/run/submit.sh` /
`script/watch_job_and_merge.sh` unless the user explicitly asks to upstream them.

Human usage summary: [`README.md`](README.md).

## When this applies

Any task involving batch submit, watch-merge, Discord notify, progress PDF, or
repo-`scratch/` job artifacts for **this user’s personal workflow**.

## Defaults (personal vs official)

| Topic | Official (git) | Personal (this clone) |
|---|---|---|
| Entry | `job/run/submit.sh` | `job/run/mysubmit` |
| Watch/merge | optional `--watch-merge` → `script/watch_job_and_merge.sh` | always; `local/watch_job_and_merge_mine.sh` |
| Progress | ROOT count only | Condor δ (this JOBID) + ROOT count |
| Notify / PDF | not in official watcher | Discord overwrite + checkHist PDF |
| SUMS log/err | paths in joblist (often repo `log/`/`err/`) | `scratch/<anaName>/sums_{log,err}/` |
| Progress hadd/PDF | n/a | `scratch/<anaName>/<jobid>/progress/` (read-only vs `rootfile/`) |
| Ignore personal paths | `.gitignore` (shared) | `.git/info/exclude` only |

## Hard rules for agents

1. **Do not modify** official `job/run/submit.sh` or `script/watch_job_and_merge.sh` for personal features.
2. **Do not put personal ignore rules in `.gitignore`**. Use `.git/info/exclude`.
3. Prefer **`./job/run/mysubmit <joblist_<anaName>.xml>`** when the user wants personal submit+merge+Discord.
4. Personal volatile root is **`$PROJECT_ROOT/scratch`**. Do not use env `SCRATCH` for this (SUMS owns that name). Optional `STAR_ANALYZER_SCRATCH=...` must be shared FS; `/tmp` is ignored and falls back to `scratch/`. Never put volatile SUMS logs or progress hadd under durable `rootfile/`. Layout: `scratch/<anaName>/sums_{log,err}/` and `scratch/<anaName>/<jobid>/...`.
5. Scope Condor / Discord / PID / scratch state by **`<anaName>/<jobid>`** (32-hex JOBID from that submit). Never treat “all my Condor jobs empty” as completion for one submit.
6. Progress hadd must **only read** subjob ROOT under `rootfile/`; write only under `scratch/.../progress/`.
7. Discord / PDF failures are **soft** (log and continue); final merge success is independent.
8. Optional mysubmit webhook: repo-root `.discord_webhook` (excluded; never commit).
9. Personal tree backup: [`../mytool/star_analyzer_backup/`](../mytool/star_analyzer_backup/) (`backup.sh` → DEST commit; `push_backup.sh` manual; hourly loop via `tmux_start.sh` / `tmux_stop.sh` / `monitor.sh`). Uses **separate** webhook file `mytool/star_analyzer_backup/.discord_webhook` (not the mysubmit one). Do not mix into the shared star-analyzer remote.
10. Before changing personal scripts, re-read this file and [`README.md`](README.md).

## Key paths

- Entry: `job/run/mysubmit`
- Watcher: `local/watch_job_and_merge_mine.sh`
- Volatile: `scratch/` (excluded)
- Backup: `mytool/star_analyzer_backup/` (DEST `/gpfs01/star/pwg/rjsaito/backup/star-analyzer` → `star-analyzer_backup.git`)
- Docs: `local/README.md`, this file
- Exclude: `.git/info/exclude` (`local/`, `job/run/mysubmit`, `scratch/`, `.discord_webhook`, `*.wip.bak`, `mytool`, …)
- Retired WIP: `backup/*.wip.bak` (not source of truth)

## Re-attach watcher (existing job)

```bash
./local/watch_job_and_merge_mine.sh \
  --runmeta job/run/runmeta/runmeta_<anaName>_<jobid>.json
```

## ACLiC build dir (trial: anaKXiFemto)

KXi runner macros redirect ROOT ACLiC output (`.so`, `.d`, etc.) away from
`analysis/` and `common/macro/`:

- **Build dir:** `$PROJECT_ROOT/.build/aclic/` via `gSystem->SetBuildDir()` in
  `analysis/run_anaKXiFemto.C` and `analysis/run_checkHistAnaKXiFemto.C`
  (before `.L ...C+`). On ROOT 5.34 (STAR), ACLiC mirrors the absolute source
  path under that dir (e.g. `.build/aclic/gpfs/mnt/.../analysis/anaKXiFemto_C.*`),
  not a flat `anaKXiFemto_C.so` at the top level.
- **Stale cleanup:** `script/singularity_run_anaKXiFemto.sh` and
  `script/singularity_checkHistAnaKXiFemto.sh` run `rm -rf .build/aclic` before
  each local Singularity run.
- **Ignore:** `.build/` is in `.git/info/exclude` (not shared `.gitignore`).
- **Scope:** KXi only for now. Do **not** upstream to `docs/ai/*` or other
  analyses until validated on batch as well.
- **Batch note:** official joblist still clears `analysis/*_C.*`; worker
  `runtime_dir` is recreated per subjob so `.build/aclic` there is ephemeral.
