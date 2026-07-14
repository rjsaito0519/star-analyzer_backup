#!/bin/bash
# Run anaLambda.C - Lambda (V0 p+ pi-) analysis with StLambdaMaker
# Usage: Run from project root: ./script/run_anaLambda.sh
#        ./script/run_anaLambda.sh [inputFile] [outputFile] [jobid] [nEvents] [configPath]
# Default: auau19 list, auau19_anaLambda_temp output, main_auau19_anaLambda.yaml

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$PROJECT_ROOT" || exit 1

INPUT_FILE="${1:-config/picoDstList/auau19GeV_lambda.list}"
OUTPUT_FILE="${2:-rootfile/auau19_anaLambda_temp/auau19_anaLambda_temp.root}"
JOBID="${3:-0}"
NEVENTS="${4:--1}"
if [ -z "${STAR_ANA_MAINCONF:-}" ] && [ -f "$PROJECT_ROOT/.current_mainconf" ]; then
  export STAR_ANA_MAINCONF=$(cat "$PROJECT_ROOT/.current_mainconf")
fi
CONFIG_PATH="${5:-$STAR_ANA_MAINCONF}"
SETUP_MAINCONF="${CONFIG_PATH:-config/mainconf/main_auau19_anaLambda.yaml}"

source ./script/setup.sh "$SETUP_MAINCONF"
export LD_LIBRARY_PATH="$PROJECT_ROOT/lib:$LD_LIBRARY_PATH"
rm -f "$PROJECT_ROOT"/analysis/anaLambda_C.* 2>/dev/null

mkdir -p "$(dirname "$OUTPUT_FILE")"

echo "=== anaLambda.C ==="
echo "Input:   $INPUT_FILE"
echo "Output:  $OUTPUT_FILE"
echo "JobID:   $JOBID"
echo "nEvents: $NEVENTS"
echo "Config:  $SETUP_MAINCONF"
echo "================================"

if [ -n "$CONFIG_PATH" ]; then
  root4star -b -q "analysis/run_anaLambda.C(\"$INPUT_FILE\",\"$OUTPUT_FILE\",\"$JOBID\",$NEVENTS,\"$CONFIG_PATH\")"
else
  root4star -b -q "analysis/run_anaLambda.C(\"$INPUT_FILE\",\"$OUTPUT_FILE\",\"$JOBID\",$NEVENTS)"
fi
