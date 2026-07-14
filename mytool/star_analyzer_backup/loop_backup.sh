#!/usr/bin/env bash
# Hourly-ish backup loop for tmux (no push — push_backup.sh stays manual).
#
# Prefer: ./tmux_start.sh  (detached session)
# Manual: ./loop_backup.sh

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CONFIG_FILE="${CONFIG_FILE:-$SCRIPT_DIR/config.env}"

if [[ -f "$CONFIG_FILE" ]]; then
  # shellcheck source=/dev/null
  source "$CONFIG_FILE"
fi

BACKUP_SCRIPT="${BACKUP_SCRIPT:-$SCRIPT_DIR/backup.sh}"
BACKUP_INTERVAL_SEC="${BACKUP_INTERVAL_SEC:-3600}"
STATE_DIR="${STATE_DIR:-$HOME/.config/star-analyzer-backup}"
LOG_FILE="${LOG_FILE:-$STATE_DIR/backup.log}"

mkdir -p "$STATE_DIR"

log() {
  local line="[$(date '+%Y-%m-%d %H:%M:%S')] LOOP: $*"
  echo "$line"
  echo "$line" >>"$LOG_FILE"
}

if [[ ! -x "$BACKUP_SCRIPT" && -f "$BACKUP_SCRIPT" ]]; then
  chmod +x "$BACKUP_SCRIPT" || true
fi

log "started on $(hostname) interval=${BACKUP_INTERVAL_SEC}s script=$BACKUP_SCRIPT"
trap 'log "stopped (signal)"; exit 0' INT TERM

while true; do
  log "tick → backup.sh"
  set +e
  "$BACKUP_SCRIPT"
  rc=$?
  set -e
  if [[ "$rc" -eq 0 ]]; then
    log "backup.sh finished ok"
  else
    log "backup.sh finished with rc=$rc (will retry after sleep)"
  fi
  log "sleep ${BACKUP_INTERVAL_SEC}s (push is manual: ./push_backup.sh)"
  sleep "$BACKUP_INTERVAL_SEC"
done
