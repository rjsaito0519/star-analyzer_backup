#!/bin/bash
# Run checkHistAnaPhi.C inside the same singularity context as batch jobs.
# Usage: ./script/singularity_checkHistAnaPhi.sh <root_file> <mainconf_path>

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
PROJECT_ROOT_REAL="$(cd "$PROJECT_ROOT" && pwd -P)"
PYTHON="$(command -v python3 2>/dev/null || command -v python 2>/dev/null)"

ROOT_FILE="${1:-}"
MAINCONF="${2:-}"

if [[ -z "$ROOT_FILE" || -z "$MAINCONF" ]]; then
  echo "Usage: $0 <root_file> <mainconf_path>" >&2
  echo "  root_file     ROOT file produced by run_anaPhi.C" >&2
  echo "  mainconf_path main config (required, e.g. config/mainconf/main_auau3p85fxt_anaPhi.yaml)" >&2
  exit 1
fi

resolve_from_project() {
  "$PYTHON" - "$PROJECT_ROOT_REAL" "$1" <<'PY'
import os
import sys

project_root = sys.argv[1]
path = sys.argv[2]
if os.path.isabs(path):
    full = path
else:
    full = os.path.join(project_root, path)
print(os.path.realpath(full))
PY
}

ROOT_FILE_REAL="$(resolve_from_project "$ROOT_FILE")"
MAINCONF_REAL="$(resolve_from_project "$MAINCONF")"
FIGURE_ROOT_REAL="$(resolve_from_project "share/figure")"

if [[ ! -f "$ROOT_FILE_REAL" ]]; then
  echo "ERROR: ROOT file not found: $ROOT_FILE_REAL" >&2
  exit 1
fi
if [[ ! -f "$MAINCONF_REAL" ]]; then
  echo "ERROR: mainconf not found: $MAINCONF_REAL" >&2
  exit 1
fi

ANA_NAME=$(cd "$PROJECT_ROOT_REAL" && "$PYTHON" script/analysis_info_helper.py --ana-name --mainconf "$MAINCONF_REAL") || exit 1
ANA_NAME="$(echo "$ANA_NAME" | xargs)"
LIBRARY_TAG=$(cd "$PROJECT_ROOT_REAL" && "$PYTHON" script/analysis_info_helper.py --library-tag --mainconf "$MAINCONF_REAL") || exit 1
LIBRARY_TAG="$(echo "$LIBRARY_TAG" | xargs)"

source "$SCRIPT_DIR/setup.sh" "$MAINCONF_REAL"
export LD_LIBRARY_PATH="$PROJECT_ROOT_REAL/lib:$LD_LIBRARY_PATH"

# Avoid mixing stale ACLiC outputs from a different runtime context.
rm -f "$PROJECT_ROOT_REAL"/common/macro/checkHistAnaPhi_C.* 2>/dev/null

CONTAINER_CMD=$(cat <<EOF
export STAR=/star/nfs4/AFS/star/packages/$LIBRARY_TAG
export STAR_HOST_SYS=$STAR_HOST_SYS
export STAR_LIB=\${STAR}/.\${STAR_HOST_SYS}/lib
export STAR_BIN=\${STAR}/.\${STAR_HOST_SYS}/bin
export PATH=\${STAR_BIN}:\${STAR}/mgr:\$PATH
export LD_LIBRARY_PATH=\${STAR_LIB}:\$LD_LIBRARY_PATH
cd "$PROJECT_ROOT_REAL"
root4star -b -q 'analysis/run_checkHistAnaPhi.C("$ROOT_FILE_REAL","$ANA_NAME","$MAINCONF_REAL")'
EOF
)

singularity exec \
  -B /gpfs:/gpfs \
  -B /star/nfs4/AFS:/star/nfs4/AFS \
  -B /home/starlib:/home/starlib \
  --env LD_LIBRARY_PATH="$LD_LIBRARY_PATH" \
  --env STAR_QA_FIGURE_ROOT="$FIGURE_ROOT_REAL" \
  /cvmfs/singularity.opensciencegrid.org/star-bnl/star-sw:latest \
  /bin/bash -lc "$CONTAINER_CMD"
