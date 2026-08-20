#!/bin/bash
# Remove loose SUMS-generated files directly under job/run (avoids "Argument list too long").
# Usage: cd job/run && ./script/cleanup_job_run.sh <anaName+jobid|--all>
# Example: ./script/cleanup_job_run.sh pp500_anaPhi63C49D52CFE76FADCE902F535133A70F
# Example: ./script/cleanup_job_run.sh --all

set -euo pipefail

usage() {
  echo "Usage: $0 <anaName+jobid|--all>" >&2
  exit 1
}

TARGET="${1:-}"
if [[ -z "$TARGET" || $# -ne 1 ]]; then
  usage
fi

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
RUN_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$RUN_DIR"

if [[ "$TARGET" == "--all" ]]; then
  # Equivalent to cleaning every anaName+jobid prefix in one directory scan.
  # Only regular files directly under job/run with a 32-hex jobid are selected;
  # runmeta/, configlog/, joblog/, and other subdirectories are preserved.
  count="$(find . -regextype posix-extended -maxdepth 1 -type f \
    -regex './[^/]*[[:xdigit:]]{32}[^/]*' -print | wc -l)"
  if [[ "$count" -eq 0 ]]; then
    echo "No loose SUMS-generated files found in $RUN_DIR"
    exit 0
  fi
  echo "Deleting $count loose SUMS-generated file(s) from $RUN_DIR"
  find . -regextype posix-extended -maxdepth 1 -type f \
    -regex './[^/]*[[:xdigit:]]{32}[^/]*' -delete
  echo "Done."
  exit 0
fi

PREFIX="$TARGET"
if [[ ! "$PREFIX" =~ ^[A-Za-z0-9_.-]+[[:xdigit:]]{32}$ ]]; then
  echo "ERROR: expected anaName followed by a 32-hex jobid, or --all: $PREFIX" >&2
  exit 1
fi

# find -delete processes one file at a time, so no argument list limit
count="$(find . -maxdepth 1 -type f -name "${PREFIX}*" -print | wc -l)"
if [[ "$count" -eq 0 ]]; then
  echo "No files matching '${PREFIX}*' in $RUN_DIR"
  exit 0
fi
echo "Deleting $count file(s) matching '${PREFIX}*' in $RUN_DIR"
find . -maxdepth 1 -type f -name "${PREFIX}*" -delete
echo "Done."
