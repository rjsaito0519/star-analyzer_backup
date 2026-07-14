#!/bin/bash
# Copy the submitted joblist XML to joblistlog/joblist_anaName_jobid.xml for reproducibility.
# Usage: ./snapshot_joblist.sh <anaName> <jobid> <path_to_submitted_xml>

set -e
anaName="${1:?Usage: $0 <anaName> <jobid> <path_to_submitted_xml>}"
jobid="${2:?Usage: $0 <anaName> <jobid> <path_to_submitted_xml>}"
submitted_xml="${3:?Usage: $0 <anaName> <jobid> <path_to_submitted_xml>}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
joblistlog_dir="$SCRIPT_DIR/joblistlog"
outfile="$joblistlog_dir/joblist_${anaName}_${jobid}.xml"

if [[ ! -f "$submitted_xml" ]]; then
  echo "ERROR: Submitted XML not found: $submitted_xml" >&2
  exit 1
fi

mkdir -p "$joblistlog_dir"
cp "$submitted_xml" "$outfile"
echo "Wrote joblist snapshot: $outfile"
