#!/bin/bash
# Run anaFemtoKaon.C inside singularity (batch-like environment).
# Usage: ./script/singularity_run_anaFemtoKaon.sh MAINCONF [inputFile] [outputFile] [jobid] [nEvents]

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
PROJECT_ROOT_REAL="$(cd "$PROJECT_ROOT" && pwd -P)"
PYTHON="$(command -v python3 2>/dev/null || command -v python 2>/dev/null)"
SINGULARITY_IMAGE="/cvmfs/singularity.opensciencegrid.org/star-bnl/star-sw:latest"

if [ -z "${STAR_ANA_MAINCONF:-}" ] && [ -f "$PROJECT_ROOT/.current_mainconf" ]; then
  export STAR_ANA_MAINCONF=$(cat "$PROJECT_ROOT/.current_mainconf")
fi
MAINCONF="${1:-$STAR_ANA_MAINCONF}"
INPUT_FILE="${2:-}"
OUTPUT_FILE="${3:-}"
JOBID="${4:-0}"
NEVENTS="${5:--1}"

if [[ -z "$MAINCONF" ]]; then
  echo "Usage: $0 MAINCONF [inputFile] [outputFile] [jobid] [nEvents]" >&2
  exit 1
fi

resolve_from_project() {
  "$PYTHON" - "$PROJECT_ROOT_REAL" "$1" <<'PY'
import os, sys
project_root = sys.argv[1]
path = sys.argv[2]
full = path if os.path.isabs(path) else os.path.join(project_root, path)
print(os.path.realpath(full))
PY
}

DEFAULT_LIST=$(cd "$PROJECT_ROOT_REAL" && "$PYTHON" script/analysis_info_helper.py --pico-dst-list --mainconf "$MAINCONF" | xargs) || exit 1
DEFAULT_OUTPUT=$(cd "$PROJECT_ROOT_REAL" && "$PYTHON" script/analysis_info_helper.py --output-rootfile --mainconf "$MAINCONF" | xargs) || exit 1
[[ -z "$INPUT_FILE" ]] && INPUT_FILE="$DEFAULT_LIST"
[[ -z "$OUTPUT_FILE" ]] && OUTPUT_FILE="$DEFAULT_OUTPUT"

MAINCONF_REAL="$(resolve_from_project "$MAINCONF")"
INPUT_FILE_REAL="$(resolve_from_project "$INPUT_FILE")"
OUTPUT_FILE_REAL="$(resolve_from_project "$OUTPUT_FILE")"

mkdir -p "$(dirname "$OUTPUT_FILE_REAL")"

LIBRARY_TAG=$(cd "$PROJECT_ROOT_REAL" && "$PYTHON" script/analysis_info_helper.py --library-tag --mainconf "$MAINCONF_REAL") || exit 1
LIBRARY_TAG="$(echo "$LIBRARY_TAG" | xargs)"

source "$SCRIPT_DIR/setup.sh" "$MAINCONF_REAL"
export LD_LIBRARY_PATH="$PROJECT_ROOT_REAL/lib:${LD_LIBRARY_PATH:-}"
rm -f "$PROJECT_ROOT_REAL"/analysis/anaFemtoKaon_C.* 2>/dev/null

CONTAINER_CMD=$(cat <<EOF
export STAR=/star/nfs4/AFS/star/packages/$LIBRARY_TAG
export STAR_HOST_SYS=$STAR_HOST_SYS
export STAR_LIB=\${STAR}/.\${STAR_HOST_SYS}/lib
export STAR_BIN=\${STAR}/.\${STAR_HOST_SYS}/bin
export PATH=\${STAR_BIN}:\${STAR}/mgr:\$PATH
export LD_LIBRARY_PATH=\${STAR_LIB}:\$LD_LIBRARY_PATH
cd "$PROJECT_ROOT_REAL"
root4star -b -q 'analysis/run_anaFemtoKaon.C("$INPUT_FILE","$OUTPUT_FILE","$JOBID",$NEVENTS,"$MAINCONF")'
EOF
)

exec singularity exec \
  -B /gpfs:/gpfs \
  -B /star/nfs4/AFS:/star/nfs4/AFS \
  -B /home/starlib:/home/starlib \
  --env LD_LIBRARY_PATH="$LD_LIBRARY_PATH" \
  "$SINGULARITY_IMAGE" \
  /bin/bash -lc "$CONTAINER_CMD"
