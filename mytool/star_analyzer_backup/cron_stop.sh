#!/usr/bin/env bash
# Remove hourly cron job and clear stale lock (if backup is not running).

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CONFIG_FILE="${CONFIG_FILE:-$SCRIPT_DIR/config.env}"

if [[ -f "$CONFIG_FILE" ]]; then
  # shellcheck source=/dev/null
  source "$CONFIG_FILE"
fi

STATE_DIR="${STATE_DIR:-$HOME/.config/star-analyzer-backup}"
LOCK_FILE="${LOCK_FILE:-$STATE_DIR/backup.lock}"
MARKER="# star-analyzer-backup"

is_backup_running() {
  if pgrep -f '[/]star_analyzer_backup/backup\.sh' >/dev/null 2>&1; then
    return 0
  fi
  if [[ -f "$LOCK_FILE" ]] && ! flock -n "$LOCK_FILE" true 2>/dev/null; then
    return 0
  fi
  return 1
}

echo "[cron] $(hostname)"
if crontab -l 2>/dev/null | grep -Fq "$MARKER"; then
  (
    crontab -l 2>/dev/null | grep -Fv "$MARKER" || true
  ) | crontab -
  echo "cron removed"
else
  echo "cron entry not found"
fi

echo "[lock] $(hostname)"
if is_backup_running; then
  echo "WARN: backup appears to be running; lock not removed"
  pgrep -af '[/]star_analyzer_backup/backup\.sh' 2>/dev/null || true
  echo "hint: wait for backup to finish, or check other login nodes"
  exit 1
fi

if [[ -f "$LOCK_FILE" ]]; then
  rm -f "$LOCK_FILE"
  echo "removed lock file: $LOCK_FILE"
else
  echo "no lock file"
fi
