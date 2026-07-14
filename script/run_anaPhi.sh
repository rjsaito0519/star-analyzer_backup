#!/bin/bash
# Run anaPhi.C - phi analysis with StPhiMaker
# Usage: ./script/run_anaPhi.sh MAINCONF [inputFile] [outputFile] [jobid] [nEvents]
#   MAINCONF  e.g. config/mainconf/main_pp500_anaPhi.yaml (required)
#   Default input/output from analysis_info (dataset.allPicoDstList, rootfile/anaName/anaName_temp.root)

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$PROJECT_ROOT" || exit 1

if [ -z "${STAR_ANA_MAINCONF:-}" ] && [ -f "$PROJECT_ROOT/.current_mainconf" ]; then
  export STAR_ANA_MAINCONF=$(cat "$PROJECT_ROOT/.current_mainconf")
fi
MAINCONF="${1:-$STAR_ANA_MAINCONF}"
MAINCONF="${MAINCONF:?Usage: ./script/run_anaPhi.sh [MAINCONF] [inputFile] [outputFile] [jobid] [nEvents]}"

DEFAULT_LIST=$(python "$SCRIPT_DIR/analysis_info_helper.py" --pico-dst-list --mainconf "$MAINCONF" | xargs) || exit 1
DEFAULT_OUTPUT=$(python "$SCRIPT_DIR/analysis_info_helper.py" --output-rootfile --mainconf "$MAINCONF" | xargs) || exit 1

INPUT_FILE="${2:-$DEFAULT_LIST}"
OUTPUT_FILE="${3:-$DEFAULT_OUTPUT}"
JOBID="${4:-0}"
NEVENTS="${5:--1}"

mkdir -p "$(dirname "$OUTPUT_FILE")"

echo "=== anaPhi.C ==="
echo "Input:   $INPUT_FILE"
echo "Output:  $OUTPUT_FILE"
echo "JobID:   $JOBID"
echo "nEvents: $NEVENTS"
echo "Config:  $MAINCONF"
echo "================================"

source ./script/setup.sh "$MAINCONF"
export LD_LIBRARY_PATH="$PROJECT_ROOT/lib:${LD_LIBRARY_PATH:-}"
rm -f "$PROJECT_ROOT"/analysis/anaPhi_C.* 2>/dev/null
root4star -b -q "analysis/run_anaPhi.C(\"$INPUT_FILE\",\"$OUTPUT_FILE\",\"$JOBID\",$NEVENTS,\"$MAINCONF\")"
