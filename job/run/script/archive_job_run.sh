#!/bin/bash
# Move all files under job/run matching anaName+jobid+* to job/run/joblog/anaName/
# (avoids "Argument list too long" by using find).
# Usage: cd job/run && ./archive_job_run.sh <anaName+jobid>
# Example: ./archive_job_run.sh pp500_anaPhi63C49D52CFE76FADCE902F535133A70F
# Example: ./archive_job_run.sh auau3p85fxt_anaPhi31FEADC2A6D9C3C0BE7D6346B8775B29

set -e
PREFIX="${1:?Usage: $0 <anaName+jobid>}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# jobid is 32 hex chars at the end; anaName is the rest
if [[ ${#PREFIX} -le 32 ]]; then
  echo "ERROR: Argument too short to be anaName+jobid (jobid is 32 chars)" >&2
  exit 1
fi
anaName="${PREFIX:0:${#PREFIX}-32}"
destDir="$SCRIPT_DIR/joblog/$anaName"
mkdir -p "$destDir"

# find -exec moves one file at a time, so no argument list limit
count="$(find . -maxdepth 1 -name "${PREFIX}*" -print | wc -l)"
if [[ "$count" -eq 0 ]]; then
  echo "No files matching '${PREFIX}*' in $SCRIPT_DIR"
  exit 0
fi
echo "Moving $count file(s) matching '${PREFIX}*' to $destDir"
find . -maxdepth 1 -name "${PREFIX}*" -exec mv {} "$destDir/" \;
echo "Done."
