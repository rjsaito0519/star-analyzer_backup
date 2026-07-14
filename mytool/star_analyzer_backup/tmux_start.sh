#!/usr/bin/env bash
# Start detached tmux session running the backup loop (cron substitute).
# Push remains manual (push_backup.sh).

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CONFIG_FILE="${CONFIG_FILE:-$SCRIPT_DIR/config.env}"

if [[ -f "$CONFIG_FILE" ]]; then
  # shellcheck source=/dev/null
  source "$CONFIG_FILE"
fi

TMUX_SESSION="${TMUX_SESSION:-star-analyzer-backup}"
LOOP_SCRIPT="${LOOP_SCRIPT:-$SCRIPT_DIR/loop_backup.sh}"
BACKUP_INTERVAL_SEC="${BACKUP_INTERVAL_SEC:-3600}"

if ! command -v tmux >/dev/null 2>&1; then
  echo "ERROR: tmux not found" >&2
  exit 1
fi

chmod +x "$LOOP_SCRIPT" 2>/dev/null || true

if tmux has-session -t "$TMUX_SESSION" 2>/dev/null; then
  echo "tmux session already running: $TMUX_SESSION"
  echo "  attach: tmux attach -t $TMUX_SESSION"
  echo "  stop:   $SCRIPT_DIR/tmux_stop.sh"
  tmux list-sessions | grep -F "$TMUX_SESSION" || true
  exit 0
fi

tmux new-session -d -s "$TMUX_SESSION" "$LOOP_SCRIPT"
echo "tmux started: $TMUX_SESSION (interval ${BACKUP_INTERVAL_SEC}s)"
echo "  attach:  tmux attach -t $TMUX_SESSION"
echo "  stop:    $SCRIPT_DIR/tmux_stop.sh"
echo "  monitor: $SCRIPT_DIR/cron_monitor.sh"
echo "  push:    $SCRIPT_DIR/push_backup.sh   # manual"
tmux list-sessions | grep -F "$TMUX_SESSION" || true
