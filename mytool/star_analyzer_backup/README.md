# star-analyzer personal backup (param_backup-style)

Separate from the shared `star-analyzer` git remote. Scripts live here under
`mytool/` (excluded from the main repo via `.git/info/exclude`).

## Flow

1. `backup.sh` — rsync SRC → DEST, commit on DEST if dirty (**no push**)
2. `push_backup.sh` — manually push DEST to `star-analyzer_backup.git`
3. Hourly loop via **tmux** (cron substitute on hosts that deny crontab):
   - `tmux_start.sh` / `tmux_stop.sh` / `monitor.sh`

## Paths (see `config.example.env`)

| Var | Default |
|---|---|
| SRC | `/gpfs01/star/pwg/rjsaito/star-analyzer` |
| DEST | `/gpfs01/star/pwg/rjsaito/backup/star-analyzer` |
| GITHUB_REMOTE | `git@github.com:rjsaito0519/star-analyzer_backup.git` |
| TMUX_SESSION | `star-analyzer-backup` |
| BACKUP_INTERVAL_SEC | `3600` |

Excludes: `rsync-exclude.txt` (rootfile, lib, SUMS clutter, secrets, …).

## Discord webhooks (two different files)

| Purpose | File |
|---|---|
| mysubmit / watch-merge | repo-root `.discord_webhook` (not used here) |
| **This** backup sync/commit | `mytool/star_analyzer_backup/.discord_webhook` |

Put one URL in `.discord_webhook` (see `.discord_webhook.example`). Both files are
rsync-excluded and listed in `backup.gitignore` so they never enter the public
backup repo.

## First-time

```bash
cd /gpfs01/star/pwg/rjsaito/star-analyzer/mytool/star_analyzer_backup
chmod +x *.sh
./prepare.sh
./backup.sh
./push_backup.sh --dry-run
./push_backup.sh          # when SSH to GitHub works
./tmux_start.sh           # hourly backup loop (no push)
./monitor.sh
```

`config.env` is local; prefer editing it instead of committing secrets.

## Scheduler (tmux)

| Command | Role |
|---|---|
| `./tmux_start.sh` | Detached session `star-analyzer-backup` running `loop_backup.sh` |
| `./tmux_stop.sh` | Kill that session |
| `./monitor.sh` | Status board (also `./cron_monitor.sh`) |
| `tmux attach -t star-analyzer-backup` | Peek at the loop |

Overlap is still guarded by `flock` inside `backup.sh`. Run the loop on **one** host only.

Legacy `cron_start.sh` / `cron_stop.sh` remain if some host allows crontab; prefer tmux on `starsub*`.

## Gotchas

- **Never** let rsync delete DEST `.git` (script uses `--filter='P .git'`).
- `starsub01` often denies `crontab` — use `./tmux_start.sh` instead.
- Push is always manual (`./push_backup.sh`).
- Backup Discord webhook is **not** the repo-root mysubmit webhook.
- tmux session dies if the host reboots — re-run `./tmux_start.sh` after login.
