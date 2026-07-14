# Personal mysubmit / watch-merge

Local-only overlay. Official `job/run/submit.sh` and `script/watch_job_and_merge.sh` stay untouched.
Ignore patterns live in `.git/info/exclude` (not shared `.gitignore`).

**Agents / other chats:** follow [`AGENTS.md`](AGENTS.md) for personal vs official rules.
Cursor loads a thin always-on pointer at `.cursor/rules/personal-local-ops.mdc` (also git-excluded).

## Requirements

- `SCRATCH` defaults to `/tmp/rjsaito` if unset (`SCRATCH=...` to override)
- Optional Discord webhook file at repo root: `.discord_webhook` (single URL line)
- `condor_q` available for queue progress (falls back to ROOT file counts if missing)
- `hadd` for hourly progress merges

## Usage

From anywhere:

```bash
./job/run/mysubmit /path/to/joblist_<anaName>.xml
```

Or:

```bash
cd job/run
./mysubmit ../joblist/joblist_<anaName>.xml
```

No `--watch-merge` flag: merge → checkHist PDF → Discord is the default for this entry.

Vanilla official path (unchanged):

```bash
cd job/run
./submit.sh --watch-merge ../joblist/joblist_<anaName>.xml
```

## Scratch layout

```
$SCRATCH/star-analyzer/
  <anaName>/
    sums_log/                 # SUMS stdout.$JOBID_N.out (stable path for Condor)
    sums_err/                 # SUMS stderr.$JOBID_N.err
    <jobid>/
      watch/                  # watchmerge_mine.log, pid, discord_message_id, condor set
      progress/               # partial_merge.root, progress/final PDFs
      log/sums_log -> ...     # convenience symlink
      err/sums_err -> ...
```

Durable (not moved to SCRATCH): `rootfile/`, `job/run/runmeta/`, configlog, joblistlog.

Repo-root `log/` / `err/` were the **old** SUMS stdout/stderr location; they may be deleted (cleanup done). Prefer `$SCRATCH/.../sums_{log,err}/` via `mysubmit`. Official `./submit.sh` with an unrebased joblist can recreate repo `log/`/`err/` if those paths are still in the XML.

## Behaviour

- Condor polling filters **this 32-hex jobid only** (safe with concurrent submits).
- Progress text updates on Condor δ; hourly scratch hadd + PDF; Discord **edits one message**.
- Final merge uses official `script/merge_root_files.csh` into `rootfile/` when Condor is empty for that jobid and ROOT counts settle (or ROOT-count fallback if `condor_q` fails).
- PDF/Discord failures are soft (logged; watcher continues).

## Dry run / checks

```bash
bash -n job/run/mysubmit
bash -n local/watch_job_and_merge_mine.sh
# Re-attach watcher to an existing submit:
./local/watch_job_and_merge_mine.sh \
  --runmeta job/run/runmeta/runmeta_<anaName>_<jobid>.json
```

## Retires

Former WIP copies were moved to `backup/*.wip.bak` (do not treat as source of truth).
