#!/usr/bin/env bash
# Manually push local backup commits to GitHub.
#
# Usage:
#   mytool/star_analyzer_backup/push_backup.sh
#   mytool/star_analyzer_backup/push_backup.sh --dry-run

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CONFIG_FILE="${CONFIG_FILE:-$SCRIPT_DIR/config.env}"
DRY_RUN=false

for arg in "$@"; do
  case "$arg" in
    --dry-run) DRY_RUN=true ;;
    -h|--help)
      echo "Usage: $0 [--dry-run]"
      exit 0
      ;;
    *)
      echo "Unknown option: $arg" >&2
      exit 1
      ;;
  esac
done

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
DISCORD_WEBHOOK_FILE="${DISCORD_WEBHOOK_FILE:-$SCRIPT_DIR/.discord_webhook}"
DISCORD_COMMIT_MSG_ID_FILE="${DISCORD_COMMIT_MSG_ID_FILE:-$STATE_DIR/discord_commit_message_id}"

mkdir -p "$STATE_DIR"

log() {
  local line="[$(date '+%Y-%m-%d %H:%M:%S')] $*"
  echo "$line"
  echo "$line" >>"$LOG_FILE"
}

read_discord_webhook() {
  if [[ ! -f "$DISCORD_WEBHOOK_FILE" ]]; then
    return 1
  fi
  local url
  url="$(grep -v '^[[:space:]]*#' "$DISCORD_WEBHOOK_FILE" | grep -v '^[[:space:]]*$' | head -n 1 || true)"
  [[ -n "$url" ]] || return 1
  echo "$url"
}

count_ahead() {
  local dest="$1"
  local branch="$2"
  if ! git -C "$dest" rev-parse "$branch" >/dev/null 2>&1; then
    echo 0
    return
  fi
  if ! git -C "$dest" rev-parse "origin/$branch" >/dev/null 2>&1; then
    git -C "$dest" rev-list --count "$branch" 2>/dev/null || echo 0
    return
  fi
  git -C "$dest" rev-list --count "origin/$branch..$branch" 2>/dev/null || echo 0
}

update_discord_commit() {
  local webhook_url="$1"
  local push_status="$2"
  local push_ok="$3"
  local last_check="$4"
  local ahead="$5"

  local commit_hash=""
  local commit_summary=""
  local last_commit_hash=""
  local last_commit_summary=""

  if git -C "$DEST" rev-parse --short HEAD >/dev/null 2>&1; then
    last_commit_hash="$(git -C "$DEST" rev-parse --short HEAD)"
    last_commit_summary="$(git -C "$DEST" show --stat --format='%s' -1 HEAD 2>/dev/null | tail -n +2 | head -5)"
  fi

  python3 "$SCRIPT_DIR/discord_status.py" commit \
    --webhook-url "$webhook_url" \
    --message-id-file "$DISCORD_COMMIT_MSG_ID_FILE" \
    --committed false \
    --run-summary "manual push ($ahead commit(s) were ahead)" \
    --last-check "$last_check" \
    --github-branch "$GITHUB_BRANCH" \
    --github-remote "$GITHUB_REMOTE" \
    --commit-hash "$commit_hash" \
    --commit-summary "$commit_summary" \
    --last-commit-hash "$last_commit_hash" \
    --last-commit-summary "$last_commit_summary" \
    --push-status "$push_status" \
    --push-ok "$push_ok" \
    --log-file "$LOG_FILE"
}

if [[ ! -d "$DEST/.git" ]]; then
  log "WARN: backup git repo not found at $DEST; attempting recover via backup.sh logic"
  # Inline light recover (same as backup.sh): reattach to GitHub
  mkdir -p "$DEST"
  git -C "$DEST" init -b "$GITHUB_BRANCH" >/dev/null 2>&1 || git -C "$DEST" init >/dev/null
  git -C "$DEST" branch -M "$GITHUB_BRANCH" 2>/dev/null || true
  if ! git -C "$DEST" remote get-url origin >/dev/null 2>&1; then
    git -C "$DEST" remote add origin "$GITHUB_REMOTE"
  else
    git -C "$DEST" remote set-url origin "$GITHUB_REMOTE"
  fi
  if ! git -C "$DEST" fetch origin "$GITHUB_BRANCH" 2>>"$LOG_FILE"; then
    log "ERROR: could not fetch $GITHUB_REMOTE — run ./backup.sh first"
    exit 1
  fi
  git -C "$DEST" checkout -B "$GITHUB_BRANCH" "origin/$GITHUB_BRANCH" >/dev/null 2>&1 \
    || git -C "$DEST" reset --mixed "origin/$GITHUB_BRANCH" >/dev/null 2>&1 \
    || true
  if [[ ! -d "$DEST/.git" ]]; then
    log "ERROR: backup git repo not found: $DEST"
    exit 1
  fi
  log "OK: recovered DEST .git from origin/$GITHUB_BRANCH"
fi

last_check="$(date '+%Y-%m-%d %H:%M:%S %z')"
log "START: manual push on $(hostname) -> origin/$GITHUB_BRANCH"

pushd "$DEST" >/dev/null

git fetch origin "$GITHUB_BRANCH" 2>>"$LOG_FILE" || log "WARN: git fetch failed (using local origin ref)"

ahead="$(count_ahead "$DEST" "$GITHUB_BRANCH")"

if [[ "$ahead" -eq 0 ]]; then
  log "PUSH: already up to date with origin/$GITHUB_BRANCH"
  popd >/dev/null

  if webhook_url="$(read_discord_webhook)"; then
    update_discord_commit "$webhook_url" "up to date with origin/$GITHUB_BRANCH" true "$last_check" 0 \
      && log "DISCORD: commit message updated" \
      || log "WARN: Discord commit message update failed"
  fi
  exit 0
fi

log "PUSH: $ahead commit(s) ahead of origin/$GITHUB_BRANCH"
git -C "$DEST" log --oneline "origin/$GITHUB_BRANCH..$GITHUB_BRANCH" 2>/dev/null | head -10 >>"$LOG_FILE" || true

if [[ "$DRY_RUN" == true ]]; then
  log "DRY-RUN: would push $ahead commit(s)"
  popd >/dev/null
  exit 0
fi

if git push -u origin "$GITHUB_BRANCH" 2>>"$LOG_FILE"; then
  log "PUSH: success ($ahead commit(s))"
  push_status="pushed $ahead commit(s) to origin/$GITHUB_BRANCH"
  push_ok=true
else
  log "ERROR: git push failed"
  push_status="push failed (see log)"
  push_ok=false
  popd >/dev/null

  if webhook_url="$(read_discord_webhook)"; then
    update_discord_commit "$webhook_url" "$push_status" false "$last_check" "$ahead" \
      || log "WARN: Discord commit message update failed"
  fi
  exit 1
fi

popd >/dev/null

if webhook_url="$(read_discord_webhook)"; then
  update_discord_commit "$webhook_url" "$push_status" true "$last_check" "$ahead" \
    && log "DISCORD: commit message updated" \
    || log "WARN: Discord commit message update failed"
fi

log "DONE"
