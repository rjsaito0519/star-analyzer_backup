#!/usr/bin/env bash
# Stop the detached tmux backup loop session.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CONFIG_FILE="${CONFIG_FILE:-$SCRIPT_DIR/config.env}"

if [[ -f "$CONFIG_FILE" ]]; then
  # shellcheck source=/dev/null
  source "$CONFIG_FILE"
fi

TMUX_SESSION="${TMUX_SESSION:-star-analyzer-backup}"

if ! command -v tmux >/dev/null 2>&1; then
  echo "ERROR: tmux not found" >&2
  exit 1
fi

if ! tmux has-session -t "$TMUX_SESSION" 2>/dev/null; then
  echo "tmux session not running: $TMUX_SESSION"
  exit 0
fi

tmux kill-session -t "$TMUX_SESSION"
echo "tmux stopped: $TMUX_SESSION"
