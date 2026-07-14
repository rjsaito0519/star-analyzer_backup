#!/usr/bin/env bash
# Colored at-a-glance monitor for star-analyzer backup.

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
TMUX_SESSION="${TMUX_SESSION:-star-analyzer-backup}"
BACKUP_INTERVAL_SEC="${BACKUP_INTERVAL_SEC:-3600}"
MARKER="# star-analyzer-backup"

if [[ -t 1 ]]; then
  RED='\033[0;31m'
  GREEN='\033[0;32m'
  YELLOW='\033[1;33m'
  CYAN='\033[0;36m'
  BOLD='\033[1m'
  DIM='\033[2m'
  NC='\033[0m'
else
  RED='' GREEN='' YELLOW='' CYAN='' BOLD='' DIM='' NC=''
fi

ok()   { printf '%bOK%b'   "$GREEN$BOLD" "$NC"; }
warn() { printf '%bWARN%b' "$YELLOW$BOLD" "$NC"; }
bad()  { printf '%bFAIL%b' "$RED$BOLD" "$NC"; }
info() { printf '%bINFO%b' "$CYAN$BOLD" "$NC"; }

row() {
  local status_fn="$1"
  local label="$2"
  local detail="$3"
  printf '  ['
  "$status_fn"
  printf '] %-14s %s\n' "$label" "$detail"
}

is_backup_running() {
  if pgrep -f '[/]star_analyzer_backup/backup\.sh' >/dev/null 2>&1; then
    return 0
  fi
  if [[ -f "$LOCK_FILE" ]] && ! flock -n "$LOCK_FILE" true 2>/dev/null; then
    return 0
  fi
  return 1
}

webhook_configured() {
  [[ -f "$DISCORD_WEBHOOK_FILE" ]] || return 1
  local url
  url="$(grep -v '^[[:space:]]*#' "$DISCORD_WEBHOOK_FILE" | grep -v '^[[:space:]]*$' | head -n 1 || true)"
  [[ -n "$url" ]]
}

printf '%b=== star-analyzer backup monitor ===%b\n' "$BOLD" "$NC"
echo "host: $(hostname)"
echo "SRC:  $SRC"
echo "DEST: $DEST"
echo "REMOTE: $GITHUB_REMOTE ($GITHUB_BRANCH)"
echo

echo "[status board]"

# DEST git
if [[ -d "$DEST/.git" ]]; then
  head_s="$(git -C "$DEST" rev-parse --short HEAD 2>/dev/null || echo '?')"
  row ok "DEST .git" "present (HEAD $head_s)"
else
  row bad "DEST .git" "MISSING — run ./backup.sh (auto-recover from GitHub)"
fi

# origin sync
if [[ -d "$DEST/.git" ]]; then
  origin_url="$(git -C "$DEST" remote get-url origin 2>/dev/null || true)"
  if [[ -z "$origin_url" ]]; then
    row bad "origin remote" "not set"
  else
    git -C "$DEST" fetch origin "$GITHUB_BRANCH" >/dev/null 2>&1 || true
    if git -C "$DEST" rev-parse "origin/$GITHUB_BRANCH" >/dev/null 2>&1; then
      ahead="$(git -C "$DEST" rev-list --count "origin/$GITHUB_BRANCH..$GITHUB_BRANCH" 2>/dev/null || echo 0)"
      behind="$(git -C "$DEST" rev-list --count "$GITHUB_BRANCH..origin/$GITHUB_BRANCH" 2>/dev/null || echo 0)"
      if [[ "$ahead" -eq 0 && "$behind" -eq 0 ]]; then
        row ok "GitHub sync" "up to date with origin/$GITHUB_BRANCH"
      elif [[ "$ahead" -gt 0 ]]; then
        row warn "GitHub sync" "$ahead commit(s) ahead — run ./push_backup.sh"
      else
        row warn "GitHub sync" "$behind commit(s) behind origin"
      fi
    else
      row warn "GitHub sync" "origin/$GITHUB_BRANCH not fetched yet"
    fi
  fi
else
  row bad "GitHub sync" "n/a (no DEST .git)"
fi

# last backup log (prefer a finished cycle)
if [[ ! -f "$LOG_FILE" ]]; then
  row warn "Last backup" "no log yet"
else
  last_done="$(tac "$LOG_FILE" | grep -E 'DONE with errors|^\[.*\] DONE$' | head -n 1 || true)"
  last_err="$(tac "$LOG_FILE" | grep -E 'ERROR:' | head -n 1 || true)"
  if [[ -n "$last_done" ]] && echo "$last_done" | grep -q 'DONE with errors'; then
    row bad "Last backup" "$last_done"
  elif [[ -n "$last_err" ]] && [[ -z "$last_done" || "$last_err" > "$last_done" ]]; then
    # crude: if newest ERROR line is more recent than DONE, show warn
    row warn "Last backup" "$last_err (after/without clean DONE — check log)"
  elif [[ -n "$last_done" ]]; then
    row ok "Last backup" "$last_done"
  else
    row info "Last backup" "$(tac "$LOG_FILE" | grep -E 'START:|SKIP:' | head -n 1 || echo none)"
  fi
fi

# lock / running (match backup.sh only, not this monitor / docs)
if pgrep -f '/star_analyzer_backup/backup\.sh([[:space:]]|$)' >/dev/null 2>&1; then
  row warn "Lock/run" "backup.sh appears RUNNING"
elif [[ -f "$LOCK_FILE" ]] && ! flock -n "$LOCK_FILE" true 2>/dev/null; then
  row warn "Lock/run" "lock held"
else
  row ok "Lock/run" "idle"
fi

# tmux loop (preferred scheduler; cron often denied on starsub*)
if ! command -v tmux >/dev/null 2>&1; then
  row bad "Tmux loop" "tmux not installed"
elif tmux has-session -t "$TMUX_SESSION" 2>/dev/null; then
  if pgrep -f '[/]star_analyzer_backup/loop_backup\.sh' >/dev/null 2>&1; then
    row ok "Tmux loop" "session $TMUX_SESSION running (interval ${BACKUP_INTERVAL_SEC}s)"
  else
    row warn "Tmux loop" "session $TMUX_SESSION exists but loop_backup.sh not seen"
  fi
else
  row warn "Tmux loop" "not running — ./tmux_start.sh"
fi

# webhook (backup-only)
if webhook_configured; then
  row ok "Webhook" "configured (backup-only file)"
else
  row warn "Webhook" "not set — edit $DISCORD_WEBHOOK_FILE"
fi

# cron / crontab permission (optional / legacy)
cron_out="$(crontab -l 2>&1)" || cron_rc=$?
cron_rc="${cron_rc:-0}"
if echo "$cron_out" | grep -qi 'not allowed'; then
  row info "Cron" "crontab DENIED on $(hostname) — use tmux instead"
elif echo "$cron_out" | grep -Fq "$MARKER"; then
  row warn "Cron" "still registered — prefer ONE scheduler (tmux OR cron)"
  echo "           $(echo "$cron_out" | grep -F "$MARKER")"
else
  row ok "Cron" "not registered (tmux is preferred)"
fi

echo
printf '%b[recent log]%b %s\n' "$DIM" "$NC" "$LOG_FILE"
if [[ -f "$LOG_FILE" ]]; then
  tail -n 12 "$LOG_FILE"
fi
echo
printf '%b[commands]%b\n' "$DIM" "$NC"
echo "  $SCRIPT_DIR/tmux_start.sh     # start hourly loop (no push)"
echo "  $SCRIPT_DIR/tmux_stop.sh"
echo "  $SCRIPT_DIR/cron_monitor.sh   # this board"
echo "  $BACKUP_SCRIPT"
echo "  $PUSH_SCRIPT                  # push is always manual"
echo "  tmux attach -t $TMUX_SESSION"
