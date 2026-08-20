#!/bin/bash
# Run anaFemtoLambda_d.C - Lambda x deuteron femtoscopy with StFemtoMakerLambdaNuclear
# Usage: ./script/run_anaFemtoLambda_d.sh [inputFile] [outputFile] [jobid] [nEvents] [configPath]
# Default: reads inputFile/outputFile from MAINCONF analysis_info

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$PROJECT_ROOT" || exit 1

if [ -z "${STAR_ANA_MAINCONF:-}" ] && [ -f "$PROJECT_ROOT/.current_mainconf" ]; then
  export STAR_ANA_MAINCONF=$(cat "$PROJECT_ROOT/.current_mainconf")
fi
CONFIG_PATH="${5:-$STAR_ANA_MAINCONF}"
SETUP_MAINCONF="${CONFIG_PATH:-config/mainconf/main_auau3p85_anaFemtoLambda_d.yaml}"

DEFAULT_LIST=$(python "$SCRIPT_DIR/analysis_info_helper.py" --pico-dst-list --mainconf "$SETUP_MAINCONF" | xargs) || exit 1
DEFAULT_OUTPUT=$(python "$SCRIPT_DIR/analysis_info_helper.py" --output-rootfile --mainconf "$SETUP_MAINCONF" | xargs) || exit 1

INPUT_FILE="${1:-$DEFAULT_LIST}"
OUTPUT_FILE="${2:-$DEFAULT_OUTPUT}"
JOBID="${3:-0}"
NEVENTS="${4:--1}"

mkdir -p "$(dirname "$OUTPUT_FILE")"

echo "=== anaFemtoLambda_d.C ==="
echo "Input:   $INPUT_FILE"
echo "Output:  $OUTPUT_FILE"
echo "JobID:   $JOBID"
echo "nEvents: $NEVENTS"
echo "Config:  $SETUP_MAINCONF"
echo "================================"

source ./script/setup.sh "$SETUP_MAINCONF"
export LD_LIBRARY_PATH="$PROJECT_ROOT/lib:${LD_LIBRARY_PATH:-}"
rm -f "$PROJECT_ROOT"/analysis/anaFemtoLambda_d_C.* 2>/dev/null

if [ -n "$CONFIG_PATH" ]; then
  root4star -b -q "analysis/run_anaFemtoLambda_d.C(\"$INPUT_FILE\",\"$OUTPUT_FILE\",\"$JOBID\",$NEVENTS,\"$CONFIG_PATH\")"
else
  root4star -b -q "analysis/run_anaFemtoLambda_d.C(\"$INPUT_FILE\",\"$OUTPUT_FILE\",\"$JOBID\",$NEVENTS)"
fi
