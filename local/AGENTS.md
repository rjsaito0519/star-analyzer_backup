# Personal / local agent notes (NOT git-managed)

This file is the **source of truth for personal submit/merge ops** in this clone.
It is under `local/` (see `.git/info/exclude`). Do **not** fold these conventions into
shared `docs/ai/*`, `PHILOSOPHY.md`, `.gitignore`, or official `job/run/submit.sh` /
`script/watch_job_and_merge.sh` unless the user explicitly asks to upstream them.

Human usage summary: [`README.md`](README.md).

## When this applies

Any task involving batch submit, watch-merge, Discord notify, progress PDF, or
`$SCRATCH` job artifacts for **this user’s personal workflow**.

## Defaults (personal vs official)

| Topic | Official (git) | Personal (this clone) |
|---|---|---|
| Entry | `job/run/submit.sh` | `job/run/mysubmit` |
| Watch/merge | optional `--watch-merge` → `script/watch_job_and_merge.sh` | always; `local/watch_job_and_merge_mine.sh` |
| Progress | ROOT count only | Condor δ (this JOBID) + ROOT count |
| Notify / PDF | not in official watcher | Discord overwrite + checkHist PDF |
| SUMS log/err | paths in joblist (often repo `log/`/`err/`) | `$SCRATCH/star-analyzer/<anaName>/sums_{log,err}/` |
| Progress hadd/PDF | n/a | `$SCRATCH/.../<jobid>/progress/` (read-only vs `rootfile/`) |
| Ignore personal paths | `.gitignore` (shared) | `.git/info/exclude` only |

## Hard rules for agents

1. **Do not modify** official `job/run/submit.sh` or `script/watch_job_and_merge.sh` for personal features.
2. **Do not put personal ignore rules in `.gitignore`**. Use `.git/info/exclude`.
3. Prefer **`./job/run/mysubmit <joblist_<anaName>.xml>`** when the user wants personal submit+merge+Discord.
4. Require **`SCRATCH`** for personal tools (`echo $SCRATCH`). Never put volatile SUMS logs or progress hadd under durable `rootfile/` as the primary store. Repo-root `log/` / `err/` are legacy paths and may be absent; personal SUMS log/err live under `$SCRATCH/star-analyzer/<anaName>/sums_{log,err}/`.
5. Scope Condor / Discord / PID / scratch state by **`<anaName>/<jobid>`** (32-hex JOBID from that submit). Never treat “all my Condor jobs empty” as completion for one submit.
6. Progress hadd must **only read** subjob ROOT under `rootfile/`; write only under `$SCRATCH/.../progress/`.
7. Discord / PDF failures are **soft** (log and continue); final merge success is independent.
8. Optional webhook: repo-root `.discord_webhook` (excluded; never commit).
9. Before changing personal scripts, re-read this file and [`README.md`](README.md).

## Key paths

- Entry: `job/run/mysubmit`
- Watcher: `local/watch_job_and_merge_mine.sh`
- Docs: `local/README.md`, this file
- Exclude: `.git/info/exclude` (`local/`, `job/run/mysubmit`, `.discord_webhook`, `*.wip.bak`, …)
- Retired WIP: `backup/*.wip.bak` (not source of truth)

## Re-attach watcher (existing job)

```bash
SCRATCH=${SCRATCH:?} ./local/watch_job_and_merge_mine.sh \
  --runmeta job/run/runmeta/runmeta_<anaName>_<jobid>.json
```
