#!/usr/bin/env bash
# Show cron registration and recent backup status.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CONFIG_FILE="${CONFIG_FILE:-$SCRIPT_DIR/config.env}"

if [[ -f "$CONFIG_FILE" ]]; then
  # shellcheck source=/dev/null
  source "$CONFIG_FILE"
fi

SRC="${SRC:-/gpfs01/star/pwg/rjsaito/star-analyzer}"
DEST="${DEST:-/gpfs01/star/pwg/rjsaito/backup/star-analyzer}"
GITHUB_REMOTE="${GITHUB_REMOTE:-git@github.com:rjsaito0519/star-analyzer_backup.git}"
GITHUB_BRANCH="${GITHUB_BRANCH:-main}"
STATE_DIR="${STATE_DIR:-$HOME/.config/star-analyzer-backup}"
LOG_FILE="${LOG_FILE:-$STATE_DIR/backup.log}"
LOCK_FILE="${LOCK_FILE:-$STATE_DIR/backup.lock}"
BACKUP_SCRIPT="${BACKUP_SCRIPT:-$SCRIPT_DIR/backup.sh}"
PUSH_SCRIPT="${PUSH_SCRIPT:-$SCRIPT_DIR/push_backup.sh}"
DISCORD_WEBHOOK_FILE="${DISCORD_WEBHOOK_FILE:-$SCRIPT_DIR/.discord_webhook}"
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

last_log_event() {
  if [[ ! -f "$LOG_FILE" ]]; then
    echo "none"
    return
  fi
  tac "$LOG_FILE" | grep -E 'DONE with errors|ERROR:|SKIP:|DONE$|START:' | head -n 1 || echo "none"
}

echo "=== star-analyzer backup monitor ==="
echo "host: $(hostname)"
echo
echo "SRC:  $SRC"
echo "DEST: $DEST"
echo "REMOTE: $GITHUB_REMOTE ($GITHUB_BRANCH)"
echo "webhook file: $DISCORD_WEBHOOK_FILE"
echo

echo "[cron]"
if crontab -l 2>/dev/null | grep -Fq "$MARKER"; then
  crontab -l 2>/dev/null | grep -F "$MARKER"
  echo "status: registered on $(hostname)"
else
  echo "status: not registered on $(hostname)"
fi
echo

echo "[lock]"
if is_backup_running; then
  echo "status: backup appears RUNNING"
  pgrep -af '[/]star_analyzer_backup/backup\.sh' 2>/dev/null || true
else
  echo "status: idle"
fi
echo "lock file: $LOCK_FILE"
echo

echo "[log] $LOG_FILE"
echo "last event: $(last_log_event)"
if [[ -f "$LOG_FILE" ]]; then
  echo "--- last 15 lines ---"
  tail -n 15 "$LOG_FILE"
fi
echo

echo "[dest git]"
if [[ -d "$DEST/.git" ]]; then
  echo "HEAD: $(git -C "$DEST" rev-parse --short HEAD 2>/dev/null || echo none)"
  echo "origin: $(git -C "$DEST" remote get-url origin 2>/dev/null || echo none)"
  if git -C "$DEST" rev-parse "origin/$GITHUB_BRANCH" >/dev/null 2>&1; then
    ahead="$(git -C "$DEST" rev-list --count "origin/$GITHUB_BRANCH..$GITHUB_BRANCH" 2>/dev/null || echo 0)"
    echo "ahead of origin/$GITHUB_BRANCH: $ahead"
  fi
else
  echo "DEST git not initialized yet"
fi
echo
echo "Commands:"
echo "  $BACKUP_SCRIPT"
echo "  $PUSH_SCRIPT"
echo "  $SCRIPT_DIR/cron_start.sh   # ONE host only"
echo "  $SCRIPT_DIR/cron_stop.sh"
