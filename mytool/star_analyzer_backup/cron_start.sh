#!/usr/bin/env bash
# Register hourly cron job for star-analyzer backup.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CONFIG_FILE="${CONFIG_FILE:-$SCRIPT_DIR/config.env}"

if [[ -f "$CONFIG_FILE" ]]; then
  # shellcheck source=/dev/null
  source "$CONFIG_FILE"
fi

BACKUP_SCRIPT="${BACKUP_SCRIPT:-$SCRIPT_DIR/backup.sh}"
MARKER="# star-analyzer-backup"
CRON_LINE="0 * * * * $BACKUP_SCRIPT $MARKER"

if ! crontab -l 2>/dev/null | grep -Fq "$MARKER"; then
  (
    crontab -l 2>/dev/null || true
    echo "$CRON_LINE"
  ) | crontab -
  echo "cron registered on $(hostname): $CRON_LINE"
else
  echo "cron already registered on $(hostname)"
fi

crontab -l | grep -F "$MARKER" || true
