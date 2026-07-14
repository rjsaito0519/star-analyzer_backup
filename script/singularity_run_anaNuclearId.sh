#!/bin/bash
# Run anaNuclearId.C inside the same singularity context as batch jobs.
# Usage: ./script/singularity_run_anaNuclearId.sh [inputFile] [outputFile] [jobid] [nEvents] [configPath]

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
PROJECT_ROOT_REAL="$(cd "$PROJECT_ROOT" && pwd -P)"
PYTHON="$(command -v python3 2>/dev/null || command -v python 2>/dev/null)"
SINGULARITY_IMAGE="/cvmfs/singularity.opensciencegrid.org/star-bnl/star-sw:latest"

INPUT_FILE="${1:-config/picoDstList/auau19GeV.list}"
OUTPUT_FILE="${2:-rootfile/auau19_anaNuclearId_temp/auau19_anaNuclearId_temp.root}"
JOBID="${3:-0}"
NEVENTS="${4:--1}"
if [ -z "${STAR_ANA_MAINCONF:-}" ] && [ -f "$PROJECT_ROOT/.current_mainconf" ]; then
  export STAR_ANA_MAINCONF=$(cat "$PROJECT_ROOT/.current_mainconf")
fi
CONFIG_PATH="${5:-$STAR_ANA_MAINCONF}"
SETUP_MAINCONF="${CONFIG_PATH:-config/mainconf/main_auau19_anaNuclearId.yaml}"

if [[ -z "$PYTHON" ]]; then
  echo "ERROR: python3 or python is required." >&2
  exit 1
fi

if ! command -v singularity >/dev/null 2>&1; then
  echo "ERROR: singularity command not found." >&2
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

MAINCONF_REAL="$(resolve_from_project "$SETUP_MAINCONF")"
INPUT_FILE_REAL="$(resolve_from_project "$INPUT_FILE")"
OUTPUT_FILE_REAL="$(resolve_from_project "$OUTPUT_FILE")"

if [[ ! -f "$MAINCONF_REAL" ]]; then
  echo "ERROR: mainconf not found: $MAINCONF_REAL" >&2
  exit 1
fi
if [[ ! -e "$INPUT_FILE_REAL" ]]; then
  echo "ERROR: input file not found: $INPUT_FILE_REAL" >&2
  exit 1
fi

mkdir -p "$(dirname "$OUTPUT_FILE_REAL")"

echo "=== anaNuclearId.C (singularity) ==="
echo "Input:   $INPUT_FILE"
echo "Output:  $OUTPUT_FILE"
echo "JobID:   $JOBID"
echo "nEvents: $NEVENTS"
echo "Config:  $SETUP_MAINCONF"
echo "================================"

LIBRARY_TAG=$(cd "$PROJECT_ROOT_REAL" && "$PYTHON" script/analysis_info_helper.py --library-tag --mainconf "$MAINCONF_REAL") || exit 1
LIBRARY_TAG="$(echo "$LIBRARY_TAG" | xargs)"

source "$SCRIPT_DIR/setup.sh" "$MAINCONF_REAL"
export LD_LIBRARY_PATH="$PROJECT_ROOT_REAL/lib:${LD_LIBRARY_PATH:-}"

rm -f "$PROJECT_ROOT_REAL"/analysis/anaNuclearId_C.* 2>/dev/null

collect_bind_mounts() {
  local -a binds=()
  local seen='|'
  local path

  add_bind() {
    local dir="$1"
    [[ -n "$dir" ]] || return 0
    case "$seen" in
      *"|$dir|"*) return 0 ;;
    esac
    seen="${seen}${dir}|"
    binds+=(-B "$dir:$dir")
  }

  add_path_binds() {
    local candidate="$1"
    local star_data_site
    [[ -n "$candidate" ]] || return 0
    candidate="${candidate%%#*}"
    candidate="$(echo "$candidate" | xargs)"
    [[ -n "$candidate" ]] || return 0

    if [[ "$candidate" == /star/data* ]]; then
      star_data_site="/$(echo "$candidate" | cut -d/ -f2-3)"
      add_bind "$star_data_site"
    elif [[ "$candidate" == /direct/* ]]; then
      add_bind "/direct"
    fi
  }

  add_bind "/gpfs"
  add_bind "/star/nfs4/AFS"
  add_bind "/home/starlib"

  add_path_binds "$INPUT_FILE_REAL"
  add_path_binds "$OUTPUT_FILE_REAL"

  if [[ -f "$INPUT_FILE_REAL" ]]; then
    while IFS= read -r path || [[ -n "$path" ]]; do
      path="${path%%#*}"
      path="$(echo "$path" | awk '{print $1}')"
      add_path_binds "$path"
    done < "$INPUT_FILE_REAL"
  fi

  printf '%s\0' "${binds[@]}"
}

mapfile -d '' SINGULARITY_BINDS < <(collect_bind_mounts)

if [[ -n "$CONFIG_PATH" ]]; then
  ROOT4STAR_CMD="analysis/run_anaNuclearId.C(\"$INPUT_FILE\",\"$OUTPUT_FILE\",\"$JOBID\",$NEVENTS,\"$CONFIG_PATH\")"
else
  ROOT4STAR_CMD="analysis/run_anaNuclearId.C(\"$INPUT_FILE\",\"$OUTPUT_FILE\",\"$JOBID\",$NEVENTS)"
fi

CONTAINER_CMD=$(cat <<EOF
export STAR=/star/nfs4/AFS/star/packages/$LIBRARY_TAG
export STAR_HOST_SYS=$STAR_HOST_SYS
export STAR_LIB=\${STAR}/.\${STAR_HOST_SYS}/lib
export STAR_BIN=\${STAR}/.\${STAR_HOST_SYS}/bin
export PATH=\${STAR_BIN}:\${STAR}/mgr:\$PATH
export LD_LIBRARY_PATH=\${STAR_LIB}:\$LD_LIBRARY_PATH
cd "$PROJECT_ROOT_REAL"
root4star -b -q '$ROOT4STAR_CMD'
EOF
)

cd "$PROJECT_ROOT_REAL"

exec singularity exec \
  --pwd "$PROJECT_ROOT_REAL" \
  "${SINGULARITY_BINDS[@]}" \
  --env LD_LIBRARY_PATH="$LD_LIBRARY_PATH" \
  "$SINGULARITY_IMAGE" \
  /bin/bash -lc "$CONTAINER_CMD"
