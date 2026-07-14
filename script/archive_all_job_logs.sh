#!/usr/bin/env bash
# Archive batch job logs under each sink directory into archive/YYYYMMDDJST/ (JST date + literal "JST").
# Example batch name: 20260330JST. If that path already exists the same JST day, uses YYYYMMDDJST_HHMMSS.
#
# Default directories (relative to project root):
#   log/  out/  err/  job/run/configlog/  job/run/joblistlog/  job/run/runmeta/
#
# Optional:
#   --joblog       Also archive files under job/run/joblog/<anaName>/ (each anaName dir gets its own archive/)
#   --test-submit  Also archive job/run/test_star_submit/log/
#
# Does NOT move loose files under job/run/ named anaName+jobid+* — use job/run/archive_job_run.sh for those.
#
# Usage:
#   cd /path/to/star-analysis && ./script/archive_all_job_logs.sh
#   ./script/archive_all_job_logs.sh -n    # dry-run
#
# Environment:
#   PROJECT_ROOT   If set, used instead of the directory above script/.. (for testing).

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="${PROJECT_ROOT:-$(cd "$SCRIPT_DIR/.." && pwd)}"

DRY_RUN=0
DO_JOBLOG=0
DO_TEST_SUBMIT=0

usage() {
  echo "Usage: $0 [-n|--dry-run] [--joblog] [--test-submit]" >&2
  exit 1
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    -n|--dry-run) DRY_RUN=1 ;;
    --joblog) DO_JOBLOG=1 ;;
    --test-submit) DO_TEST_SUBMIT=1 ;;
    -h|--help) usage ;;
    *) echo "Unknown option: $1" >&2; usage ;;
  esac
  shift
done

# JST batch label: YYYYMMDDJST, or YYYYMMDDJST_HHMMSS if base already exists under D/archive/
archive_subdir_for() {
  local parent_dir="$1"
  local base
  base="$(TZ=Asia/Tokyo date +%Y%m%d)JST"
  local dest="$parent_dir/archive/$base"
  if [[ -e "$dest" ]]; then
    base="${base}_$(TZ=Asia/Tokyo date +%H%M%S)"
    dest="$parent_dir/archive/$base"
  fi
  printf '%s' "$base"
}

# Move all regular files directly under dir into dir/archive/<batch>/ (batch from archive_subdir_for)
archive_dir() {
  local dir="${1%/}"
  local label="${2:-$dir}"

  if [[ ! -d "$dir" ]]; then
    echo "SKIP (not a directory): $label"
    return 0
  fi

  local count
  count="$(find "$dir" -mindepth 1 -maxdepth 1 -type f 2>/dev/null | wc -l | tr -d ' ')"
  if [[ "$count" -eq 0 ]]; then
    echo "SKIP (no files): $label"
    return 0
  fi

  local sub
  sub="$(archive_subdir_for "$dir")"
  local dest="$dir/archive/$sub"

  if [[ "$DRY_RUN" -eq 1 ]]; then
    echo "[dry-run] mkdir -p $(printf '%q' "$dest")"
    find "$dir" -mindepth 1 -maxdepth 1 -type f -print0 | while IFS= read -r -d '' f; do
      echo "[dry-run] mv $(printf '%q' "$f") $(printf '%q' "$dest/")"
    done
    return 0
  fi

  mkdir -p "$dest"
  find "$dir" -mindepth 1 -maxdepth 1 -type f -exec mv {} "$dest/" \;
  echo "OK: moved $count file(s) from $label -> $dest"
}

DEFAULT_DIRS=(
  "log"
  "out"
  "err"
  "job/run/configlog"
  "job/run/joblistlog"
  "job/run/runmeta"
)

echo "PROJECT_ROOT=$PROJECT_ROOT"
echo "Batch naming: TZ=Asia/Tokyo -> archive/<YYYYMMDD>JST[/ _HHMMSS if collision]"
echo ""

for rel in "${DEFAULT_DIRS[@]}"; do
  archive_dir "$PROJECT_ROOT/$rel" "$rel"
done

if [[ "$DO_TEST_SUBMIT" -eq 1 ]]; then
  archive_dir "$PROJECT_ROOT/job/run/test_star_submit/log" "job/run/test_star_submit/log"
fi

if [[ "$DO_JOBLOG" -eq 1 ]]; then
  joblog_root="$PROJECT_ROOT/job/run/joblog"
  if [[ ! -d "$joblog_root" ]]; then
    echo "SKIP (not a directory): job/run/joblog"
  else
    shopt -s nullglob
    for sub in "$joblog_root"/*/; do
      [[ -d "$sub" ]] || continue
      name="$(basename "$sub")"
      [[ "$name" == "archive" ]] && continue
      archive_dir "$sub" "job/run/joblog/$name"
    done
    shopt -u nullglob
  fi
fi

echo "Done."
