# star-analyzer personal backup (param_backup-style)

Separate from the shared `star-analyzer` git remote. Scripts live here under
`mytool/` (excluded from the main repo via `.git/info/exclude`).

## Flow

1. `backup.sh` — rsync SRC → DEST, commit on DEST if dirty (**no push**)
2. `push_backup.sh` — manually push DEST to `star-analyzer_backup.git`
3. Optional hourly cron on **one** host: `cron_start.sh`

## Paths (see `config.example.env`)

| Var | Default |
|---|---|
| SRC | `/gpfs01/star/pwg/rjsaito/star-analyzer` |
| DEST | `/gpfs01/star/pwg/rjsaito/backup/star-analyzer` |
| GITHUB_REMOTE | `git@github.com:rjsaito0519/star-analyzer_backup.git` |

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
# optional later:
# ./cron_start.sh
# ./cron_monitor.sh
```

`config.env` is local; prefer editing it instead of committing secrets.

## Gotchas

- **Never** let rsync delete DEST `.git` (script uses `--filter='P .git'`).
- `starsub01` may deny `crontab` — monitor shows that in red; run `backup.sh` manually or cron on a host that allows it.
- Backup Discord webhook is **not** the repo-root mysubmit webhook.
