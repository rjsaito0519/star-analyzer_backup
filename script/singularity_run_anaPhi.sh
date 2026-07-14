#!/bin/bash
# Run anaPhi.C inside the same singularity context as batch jobs.
# Usage: ./script/singularity_run_anaPhi.sh MAINCONF [inputFile] [outputFile] [jobid] [nEvents]

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
  echo "  MAINCONF    main config (required, e.g. config/mainconf/main_auau3p85fxt_anaPhi.yaml)" >&2
  echo "  inputFile   picoDst list or single input (default from analysis_info)" >&2
  echo "  outputFile  output ROOT file (default from analysis_info)" >&2
  echo "  jobid       job id passed to anaPhi (default: 0)" >&2
  echo "  nEvents     max events, -1 for all (default: -1)" >&2
  exit 1
fi

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

DEFAULT_LIST=$(cd "$PROJECT_ROOT_REAL" && "$PYTHON" script/analysis_info_helper.py --pico-dst-list --mainconf "$MAINCONF" | xargs) || exit 1
DEFAULT_OUTPUT=$(cd "$PROJECT_ROOT_REAL" && "$PYTHON" script/analysis_info_helper.py --output-rootfile --mainconf "$MAINCONF" | xargs) || exit 1

if [[ -z "$INPUT_FILE" ]]; then
  INPUT_FILE="$DEFAULT_LIST"
fi
if [[ -z "$OUTPUT_FILE" ]]; then
  OUTPUT_FILE="$DEFAULT_OUTPUT"
fi

MAINCONF_REAL="$(resolve_from_project "$MAINCONF")"
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

echo "=== anaPhi.C (singularity) ==="
echo "Input:   $INPUT_FILE"
echo "Output:  $OUTPUT_FILE"
echo "JobID:   $JOBID"
echo "nEvents: $NEVENTS"
echo "Config:  $MAINCONF"
echo "================================"

LIBRARY_TAG=$(cd "$PROJECT_ROOT_REAL" && "$PYTHON" script/analysis_info_helper.py --library-tag --mainconf "$MAINCONF_REAL") || exit 1
LIBRARY_TAG="$(echo "$LIBRARY_TAG" | xargs)"

source "$SCRIPT_DIR/setup.sh" "$MAINCONF_REAL"
export LD_LIBRARY_PATH="$PROJECT_ROOT_REAL/lib:${LD_LIBRARY_PATH:-}"

rm -f "$PROJECT_ROOT_REAL"/analysis/anaPhi_C.* 2>/dev/null

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

CONTAINER_CMD=$(cat <<EOF
export STAR=/star/nfs4/AFS/star/packages/$LIBRARY_TAG
export STAR_HOST_SYS=$STAR_HOST_SYS
export STAR_LIB=\${STAR}/.\${STAR_HOST_SYS}/lib
export STAR_BIN=\${STAR}/.\${STAR_HOST_SYS}/bin
export PATH=\${STAR_BIN}:\${STAR}/mgr:\$PATH
export LD_LIBRARY_PATH=\${STAR_LIB}:\$LD_LIBRARY_PATH
cd "$PROJECT_ROOT_REAL"
root4star -b -q 'analysis/run_anaPhi.C("$INPUT_FILE","$OUTPUT_FILE","$JOBID",$NEVENTS,"$MAINCONF")'
EOF
)

exec singularity exec \
  "${SINGULARITY_BINDS[@]}" \
  --env LD_LIBRARY_PATH="$LD_LIBRARY_PATH" \
  "$SINGULARITY_IMAGE" \
  /bin/bash -lc "$CONTAINER_CMD"
