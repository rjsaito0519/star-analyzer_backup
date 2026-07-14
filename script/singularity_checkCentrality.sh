#!/bin/bash
# Run checkCentrality via batch-like singularity (same pattern as singularity_checkHistAnaPhi.sh).
# Usage: ./script/singularity_checkCentrality.sh <picoDst_or_list> <mainconf_path> [output.root] [maxEvents]

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
PROJECT_ROOT_REAL="$(cd "$PROJECT_ROOT" && pwd -P)"
PYTHON="$(command -v python3 2>/dev/null || command -v python 2>/dev/null)"

INPUT="${1:-}"
MAINCONF="${2:-}"
OUTPUT="${3:-centrality_qa.root}"
MAX_EVENTS="${4:--1}"

if [[ -z "$INPUT" || -z "$MAINCONF" ]]; then
  echo "Usage: $0 <picoDst_or_list> <mainconf_path> [output.root] [maxEvents]" >&2
  exit 1
fi

resolve_from_project() {
  "$PYTHON" - "$PROJECT_ROOT_REAL" "$1" <<'PY'
import os, sys
project_root, path = sys.argv[1], sys.argv[2]
full = path if os.path.isabs(path) else os.path.join(project_root, path)
print(os.path.realpath(full))
PY
}

INPUT_REAL="$(resolve_from_project "$INPUT")"
MAINCONF_REAL="$(resolve_from_project "$MAINCONF")"

if [[ ! -f "$MAINCONF_REAL" ]]; then
  echo "ERROR: mainconf not found: $MAINCONF_REAL" >&2
  exit 1
fi

LIBRARY_TAG=$(cd "$PROJECT_ROOT_REAL" && "$PYTHON" script/analysis_info_helper.py --library-tag --mainconf "$MAINCONF_REAL") || exit 1
LIBRARY_TAG="$(echo "$LIBRARY_TAG" | xargs)"

source "$SCRIPT_DIR/setup.sh" "$MAINCONF_REAL"
export LD_LIBRARY_PATH="$PROJECT_ROOT_REAL/lib:$LD_LIBRARY_PATH"
rm -f "$PROJECT_ROOT_REAL"/common/macro/checkCentrality_C.* 2>/dev/null

CONTAINER_CMD=$(cat <<EOF
export STAR=/star/nfs4/AFS/star/packages/$LIBRARY_TAG
export STAR_HOST_SYS=$STAR_HOST_SYS
export STAR_LIB=\${STAR}/.\${STAR_HOST_SYS}/lib
export STAR_BIN=\${STAR}/.\${STAR_HOST_SYS}/bin
export PATH=\${STAR_BIN}:\${STAR}/mgr:\$PATH
export LD_LIBRARY_PATH=\${STAR_LIB}:\$LD_LIBRARY_PATH
cd "$PROJECT_ROOT_REAL"
root4star -b -q 'analysis/run_checkCentrality.C("$INPUT_REAL","$MAINCONF_REAL","$OUTPUT",$MAX_EVENTS)'
EOF
)

singularity exec \
  -B /gpfs:/gpfs \
  -B /star/nfs4/AFS:/star/nfs4/AFS \
  -B /home/starlib:/home/starlib \
  --env LD_LIBRARY_PATH="$LD_LIBRARY_PATH" \
  /cvmfs/singularity.opensciencegrid.org/star-bnl/star-sw:latest \
  /bin/bash -lc "$CONTAINER_CMD"
