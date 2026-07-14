#!/usr/bin/env bash
# Alias for cron_monitor.sh (tmux era — same status board).
exec "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/cron_monitor.sh" "$@"
