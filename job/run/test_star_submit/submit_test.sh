#!/bin/bash
# Submit minimal test job (no root4star). Run from job/run: ./test_star_submit/submit_test.sh
set -e
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"
# Project root: job/run/test_star_submit -> run -> job -> star-analysis
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"
mkdir -p log result
FILLED="test_minimal_filled.xml"
sed "s|__PROJECT_ROOT__|$PROJECT_ROOT|g" test_minimal.xml > "$FILLED"
echo "Submitting minimal test job (PROJECT_ROOT=$PROJECT_ROOT)"
star-submit "$FILLED"
echo "Check: result/ for test_result_<JOBID>.txt, log/ for stdout/stderr"
