#!/usr/bin/env bash
# Sync star-analyzer working tree to a personal backup git repo and commit on changes.
# Does NOT push (use push_backup.sh).
#
# Setup:
#   1. chmod +x mytool/star_analyzer_backup/*.sh
#   2. cp config.example.env config.env  (already present)
#   3. Optional: edit .discord_webhook (backup-only URL)
#   4. ./prepare.sh ; ./backup.sh ; ./push_backup.sh
#   5. Optional cron on ONE host: ./cron_start.sh

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
DISCORD_WEBHOOK_FILE="${DISCORD_WEBHOOK_FILE:-$SCRIPT_DIR/.discord_webhook}"
RSYNC_EXCLUDE="${RSYNC_EXCLUDE:-$SCRIPT_DIR/rsync-exclude.txt}"
GITIGNORE_TEMPLATE="${GITIGNORE_TEMPLATE:-$SCRIPT_DIR/backup.gitignore}"
DISCORD_SYNC_MSG_ID_FILE="${DISCORD_SYNC_MSG_ID_FILE:-$STATE_DIR/discord_sync_message_id}"
DISCORD_COMMIT_MSG_ID_FILE="${DISCORD_COMMIT_MSG_ID_FILE:-$STATE_DIR/discord_commit_message_id}"

mkdir -p "$STATE_DIR" "$DEST"

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

check_source_branch() {
  local current_branch="unknown"
  if [[ -d "$SRC/.git" || -f "$SRC/.git" ]]; then
    current_branch="$(git -C "$SRC" rev-parse --abbrev-ref HEAD 2>/dev/null || echo unknown)"
  fi
  echo "$current_branch"
}

ensure_dest_gitignore() {
  if [[ ! -f "$GITIGNORE_TEMPLATE" ]]; then
    log "WARN: gitignore template not found: $GITIGNORE_TEMPLATE"
    return
  fi
  if [[ ! -f "$DEST/.gitignore" ]]; then
    cp "$GITIGNORE_TEMPLATE" "$DEST/.gitignore"
    log "INIT: installed $DEST/.gitignore"
  fi
}

init_dest_repo() {
  if [[ ! -d "$DEST/.git" ]]; then
    git -C "$DEST" init -b "$GITHUB_BRANCH" >/dev/null 2>&1 \
      || git -C "$DEST" init >/dev/null
    git -C "$DEST" branch -M "$GITHUB_BRANCH" 2>/dev/null || true
    log "INIT: created git repo at $DEST"
  fi

  if ! git -C "$DEST" remote get-url origin >/dev/null 2>&1; then
    git -C "$DEST" remote add origin "$GITHUB_REMOTE"
    log "INIT: added origin $GITHUB_REMOTE"
  fi

  ensure_dest_gitignore
}

count_rsync_changes() {
  local output
  output="$(rsync -a --delete --dry-run --itemize-changes \
    --exclude-from="$RSYNC_EXCLUDE" \
    "$SRC/" "$DEST/" 2>/dev/null || true)"

  if [[ -z "$output" ]]; then
    echo 0
    return
  fi
  echo "$output" | grep -c '^[<>ch.*]' || echo 0
}

do_rsync() {
  rsync -a --delete \
    --exclude-from="$RSYNC_EXCLUDE" \
    "$SRC/" "$DEST/"
}

update_discord_sync() {
  local webhook_url="$1"
  local sync_ok="$2"
  local sync_status="$3"
  local last_check="$4"
  local source_branch="$5"
  local files_changed="$6"

  python3 "$SCRIPT_DIR/discord_status.py" sync \
    --webhook-url "$webhook_url" \
    --message-id-file "$DISCORD_SYNC_MSG_ID_FILE" \
    --sync-ok "$sync_ok" \
    --sync-status "$sync_status" \
    --last-check "$last_check" \
    --source-branch "$source_branch" \
    --files-changed "$files_changed" \
    --source "$SRC" \
    --destination "$DEST" \
    --log-file "$LOG_FILE"
}

update_discord_commit() {
  local webhook_url="$1"
  local committed="$2"
  local run_summary="$3"
  local last_check="$4"
  local commit_hash="$5"
  local commit_summary="$6"
  local last_commit_hash="$7"
  local last_commit_summary="$8"
  local push_status="$9"
  local push_ok="${10}"

  python3 "$SCRIPT_DIR/discord_status.py" commit \
    --webhook-url "$webhook_url" \
    --message-id-file "$DISCORD_COMMIT_MSG_ID_FILE" \
    --committed "$committed" \
    --run-summary "$run_summary" \
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

do_backup() {
  local last_check
  last_check="$(date '+%Y-%m-%d %H:%M:%S %z')"

  if [[ ! -d "$SRC" ]]; then
    log "ERROR: source not found: $SRC"
    exit 1
  fi
  if [[ ! -f "$RSYNC_EXCLUDE" ]]; then
    log "ERROR: rsync exclude list not found: $RSYNC_EXCLUDE"
    exit 1
  fi

  local source_branch
  source_branch="$(check_source_branch)"

  log "START: rsync $SRC -> $DEST (src_branch=$source_branch)"

  local webhook_url=""
  if webhook_url="$(read_discord_webhook)"; then
    :
  else
    log "INFO: backup Discord webhook not configured ($DISCORD_WEBHOOK_FILE)"
  fi

  local sync_ok=true
  local sync_status="synced"
  local files_changed=0
  local committed=false
  local commit_hash=""
  local commit_summary=""
  local last_commit_hash=""
  local last_commit_summary=""
  local push_status="not pushed (manual)"
  local push_ok=true
  local run_summary="no changes"
  local ahead=0

  files_changed="$(count_rsync_changes)"

  if ! init_dest_repo; then
    sync_ok=false
    sync_status="git init failed"
    log "ERROR: failed to initialize destination git repo"
  elif ! do_rsync; then
    sync_ok=false
    sync_status="rsync failed"
    log "ERROR: rsync failed"
  else
    if [[ "$files_changed" -eq 0 ]]; then
      sync_status="synced (no file changes)"
      log "OK: rsync completed, no file changes detected"
    else
      sync_status="synced ($files_changed itemized changes)"
      log "OK: rsync completed, $files_changed itemized changes"
    fi

    pushd "$DEST" >/dev/null

    if [[ -n "$(git status --porcelain)" ]]; then
      git add -A
      local commit_msg="auto backup $(date '+%Y-%m-%d %H:%M:%S')"
      git commit -m "$commit_msg" --quiet
      committed=true
      commit_hash="$(git rev-parse --short HEAD)"
      commit_summary="$(git show --stat --format='%s' -1 HEAD 2>/dev/null | tail -n +2 | head -5)"
      run_summary="new commit created (local only)"
      log "COMMIT: $commit_hash $commit_msg"
    else
      run_summary="no changes to commit"
      log "OK: no changes to commit"
      if git rev-parse --short HEAD >/dev/null 2>&1; then
        last_commit_hash="$(git rev-parse --short HEAD)"
        last_commit_summary="$(git show --stat --format='%s' -1 HEAD 2>/dev/null | tail -n +2 | head -5)"
      fi
    fi

    if git rev-parse "origin/$GITHUB_BRANCH" >/dev/null 2>&1; then
      ahead="$(git rev-list --count "origin/$GITHUB_BRANCH..$GITHUB_BRANCH" 2>/dev/null || echo 0)"
    elif git rev-parse "$GITHUB_BRANCH" >/dev/null 2>&1; then
      ahead="$(git rev-list --count "$GITHUB_BRANCH" 2>/dev/null || echo 0)"
    fi

    if [[ "$ahead" -gt 0 ]]; then
      push_status="$ahead commit(s) ahead, not pushed (run push_backup.sh)"
      log "INFO: $ahead commit(s) waiting for manual push"
    else
      push_status="up to date with origin/$GITHUB_BRANCH"
    fi

    popd >/dev/null
  fi

  if [[ -n "$webhook_url" ]]; then
    if update_discord_sync "$webhook_url" "$sync_ok" "$sync_status" "$last_check" \
      "$source_branch" "$files_changed"; then
      log "DISCORD: sync message updated"
    else
      log "WARN: Discord sync message update failed"
    fi

    if update_discord_commit "$webhook_url" "$committed" "$run_summary" "$last_check" \
      "$commit_hash" "$commit_summary" "$last_commit_hash" "$last_commit_summary" \
      "$push_status" "$push_ok"; then
      log "DISCORD: commit message updated"
    else
      log "WARN: Discord commit message update failed"
    fi
  fi

  if [[ "$sync_ok" == false ]]; then
    log "DONE with errors"
    exit 1
  fi

  log "DONE"
}

run_with_lock() {
  exec 9>>"$LOCK_FILE"
  if ! flock -n 9; then
    log "SKIP: another backup is already running (lock=$LOCK_FILE)"
    exit 0
  fi
  log "LOCK: acquired on $(hostname)"
  do_backup
}

run_with_lock
