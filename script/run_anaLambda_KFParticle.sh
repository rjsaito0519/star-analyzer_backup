#!/bin/bash
# Run the KFParticle Lambda analysis from the project root.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$PROJECT_ROOT"

INPUT_FILE="${1:-config/picoDstList/auau19GeV.list}"
OUTPUT_FILE="${2:-rootfile/auau19_anaLambda_KFParticle_temp/auau19_anaLambda_KFParticle_temp.root}"
JOBID="${3:-0}"
NEVENTS="${4:--1}"
CONFIG_PATH="${5:-config/mainconf/main_auau19_anaLambda_KFParticle.yaml}"

source ./script/setup.sh "$CONFIG_PATH"
export LD_LIBRARY_PATH="$PROJECT_ROOT/lib:${LD_LIBRARY_PATH:-}"
mkdir -p "$(dirname "$OUTPUT_FILE")"

echo "=== anaLambda_KFParticle ==="
echo "Input:   $INPUT_FILE"
echo "Output:  $OUTPUT_FILE"
echo "JobID:   $JOBID"
echo "nEvents: $NEVENTS"
echo "Config:  $CONFIG_PATH"

root4star -b -q \
  "analysis/run_anaLambda_KFParticle.C(\"$INPUT_FILE\",\"$OUTPUT_FILE\",\"$JOBID\",$NEVENTS,\"$CONFIG_PATH\")"
