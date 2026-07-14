#!/bin/bash
# Remove all files under job/run matching anaName+jobid+* (avoids "Argument list too long").
# Usage: cd job/run && ./cleanup_job_run.sh <anaName+jobid>
# Example: ./cleanup_job_run.sh pp500_anaPhi63C49D52CFE76FADCE902F535133A70F
# Example: ./cleanup_job_run.sh auau3p85fxt_anaPhi31FEADC2A6D9C3C0BE7D6346B8775B29

set -e
PREFIX="${1:?Usage: $0 <anaName+jobid>}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# find -delete processes one file at a time, so no argument list limit
count="$(find . -maxdepth 1 -name "${PREFIX}*" -print | wc -l)"
if [[ "$count" -eq 0 ]]; then
  echo "No files matching '${PREFIX}*' in $SCRIPT_DIR"
  exit 0
fi
echo "Deleting $count file(s) matching '${PREFIX}*' in $SCRIPT_DIR"
find . -maxdepth 1 -name "${PREFIX}*" -delete
echo "Done."
