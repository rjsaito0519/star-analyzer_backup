#!/bin/bash
# Run anaFemtoPhiDeuteron.C - phi-p femtoscopy with StFemtoMaker
# Usage: ./script/run_anaFemtoPhiDeuteron.sh MAINCONF [inputFile] [outputFile] [jobid] [nEvents]

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$PROJECT_ROOT" || exit 1

if [ -z "${STAR_ANA_MAINCONF:-}" ] && [ -f "$PROJECT_ROOT/.current_mainconf" ]; then
  export STAR_ANA_MAINCONF=$(cat "$PROJECT_ROOT/.current_mainconf")
fi
MAINCONF="${1:-$STAR_ANA_MAINCONF}"
MAINCONF="${MAINCONF:?Usage: ./script/run_anaFemtoPhiDeuteron.sh [MAINCONF] [inputFile] [outputFile] [jobid] [nEvents]}"

DEFAULT_LIST=$(python "$SCRIPT_DIR/analysis_info_helper.py" --pico-dst-list --mainconf "$MAINCONF" | xargs) || exit 1
DEFAULT_OUTPUT=$(python "$SCRIPT_DIR/analysis_info_helper.py" --output-rootfile --mainconf "$MAINCONF" | xargs) || exit 1

INPUT_FILE="${2:-$DEFAULT_LIST}"
OUTPUT_FILE="${3:-$DEFAULT_OUTPUT}"
JOBID="${4:-0}"
NEVENTS="${5:--1}"

mkdir -p "$(dirname "$OUTPUT_FILE")"

echo "=== anaFemtoPhiDeuteron.C ==="
echo "Input:   $INPUT_FILE"
echo "Output:  $OUTPUT_FILE"
echo "JobID:   $JOBID"
echo "nEvents: $NEVENTS"
echo "Config:  $MAINCONF"
echo "================================"

source ./script/setup.sh "$MAINCONF"
export LD_LIBRARY_PATH="$PROJECT_ROOT/lib:${LD_LIBRARY_PATH:-}"
rm -f "$PROJECT_ROOT"/analysis/anaFemtoPhiDeuteron_C.* 2>/dev/null
root4star -b -q "analysis/run_anaFemtoPhiDeuteron.C(\"$INPUT_FILE\",\"$OUTPUT_FILE\",\"$JOBID\",$NEVENTS,\"$MAINCONF\")"
