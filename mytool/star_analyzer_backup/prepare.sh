#!/usr/bin/env bash
# Stop schedulers, clear stale lock, and show next steps (does not run backup).

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CONFIG_FILE="${CONFIG_FILE:-$SCRIPT_DIR/config.env}"

if [[ -f "$CONFIG_FILE" ]]; then
  # shellcheck source=/dev/null
  source "$CONFIG_FILE"
fi

STATE_DIR="${STATE_DIR:-$HOME/.config/star-analyzer-backup}"
LOCK_FILE="${LOCK_FILE:-$STATE_DIR/backup.lock}"
BACKUP_SCRIPT="${BACKUP_SCRIPT:-$SCRIPT_DIR/backup.sh}"
PUSH_SCRIPT="${PUSH_SCRIPT:-$SCRIPT_DIR/push_backup.sh}"

echo "=== star-analyzer backup prepare ==="
echo "host: $(hostname)"
echo

echo "[1/3] stop tmux loop (and legacy cron) on this host"
"$SCRIPT_DIR/tmux_stop.sh" || true
"$SCRIPT_DIR/cron_stop.sh" || true
echo

echo "[2/3] clear stale lock if idle"
if [[ -f "$LOCK_FILE" ]] && flock -n "$LOCK_FILE" true 2>/dev/null; then
  rm -f "$LOCK_FILE"
  echo "removed stale $LOCK_FILE"
else
  echo "lock ok / not held"
fi
echo

echo "[3/3] confirm backup is not running"
if pgrep -af '[/]star_analyzer_backup/backup\.sh' >/dev/null 2>&1; then
  echo "WARN: backup.sh is still running:"
  pgrep -af '[/]star_analyzer_backup/backup\.sh'
  exit 1
fi
echo "no running backup.sh on $(hostname)"
echo

echo "=== next steps (run manually) ==="
echo "1. Test backup once:"
echo "   $BACKUP_SCRIPT"
echo
echo "2. Push to GitHub when ready (always manual):"
echo "   $PUSH_SCRIPT --dry-run"
echo "   $PUSH_SCRIPT"
echo
echo "3. Start hourly tmux loop on ONE host:"
echo "   $SCRIPT_DIR/tmux_start.sh"
echo "   $SCRIPT_DIR/tmux_stop.sh"
echo
echo "4. Monitor:"
echo "   $SCRIPT_DIR/monitor.sh"
echo
echo "Webhook (optional, backup-only, not mysubmit):"
echo "   $SCRIPT_DIR/.discord_webhook"
