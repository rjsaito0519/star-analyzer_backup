#!/bin/bash
# Build and run share/femtocalc/fit_correlation inside singularity (SL7-like runtime).
# Usage: ./script/singularity_run_fitCorrelation.sh <root_file> <mainconf_path> [hist_name]
#
# share/ is often a symlink (e.g. to sharelocal on GPFS). Resolve real paths before use.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
PROJECT_ROOT_REAL="$(cd "$PROJECT_ROOT" && pwd -P)"
SINGULARITY_IMAGE="/cvmfs/singularity.opensciencegrid.org/star-bnl/star-sw:latest"

PYTHON="$(command -v python3 2>/dev/null || command -v python 2>/dev/null)"
ROOT_FILE="${1:-}"
MAINCONF="${2:-}"
HIST_NAME="${3:-hCF}"

if [[ -z "$ROOT_FILE" || -z "$MAINCONF" ]]; then
  echo "Usage: $0 <root_file> <mainconf_path> [hist_name]" >&2
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

ROOT_FILE_REAL="$(resolve_from_project "$ROOT_FILE")"
MAINCONF_REAL="$(resolve_from_project "$MAINCONF")"
FEMTOCALC_DIR="$(readlink -f "$PROJECT_ROOT/share/femtocalc")"
FIT_SRC="$FEMTOCALC_DIR/fit_correlation.cpp"
FIT_BIN="$FEMTOCALC_DIR/fit_correlation"
OUTPUT_PDF="$PROJECT_ROOT_REAL/cf_fit.pdf"

if [[ ! -f "$ROOT_FILE_REAL" ]]; then
  echo "Error: file not found: $ROOT_FILE_REAL" >&2
  exit 1
fi
if [[ ! -f "$MAINCONF_REAL" ]]; then
  echo "Error: mainconf not found: $MAINCONF_REAL" >&2
  exit 1
fi
if [[ ! -f "$FIT_SRC" ]]; then
  echo "Error: source not found: $FIT_SRC" >&2
  exit 1
fi

LIBRARY_TAG="$(cd "$PROJECT_ROOT_REAL" && "$PYTHON" script/analysis_info_helper.py --library-tag --mainconf "$MAINCONF_REAL" | xargs)" || exit 1
source "$SCRIPT_DIR/setup.sh" "$MAINCONF_REAL"
export LD_LIBRARY_PATH="$PROJECT_ROOT_REAL/lib:${LD_LIBRARY_PATH:-}"

CONTAINER_CMD=$(cat <<EOF
export STAR=/star/nfs4/AFS/star/packages/$LIBRARY_TAG
export STAR_HOST_SYS=$STAR_HOST_SYS
export STAR_LIB=\${STAR}/.\${STAR_HOST_SYS}/lib
export STAR_BIN=\${STAR}/.\${STAR_HOST_SYS}/bin
export PATH=\${STAR_BIN}:\${STAR}/mgr:\$PATH
export LD_LIBRARY_PATH=\${STAR_LIB}:\$LD_LIBRARY_PATH
cd "$FEMTOCALC_DIR"
if [[ ! -x "$FIT_BIN" || "$FIT_SRC" -nt "$FIT_BIN" || "$FEMTOCALC_DIR/FemtoModels.hpp" -nt "$FIT_BIN" ]]; then
  echo "Building $FIT_BIN ..."
  g++ -O2 -std=c++11 "$FIT_SRC" -o "$FIT_BIN" \$(root-config --cflags --libs)
fi
"$FIT_BIN" "$ROOT_FILE_REAL" "$HIST_NAME" "$OUTPUT_PDF"
EOF
)

echo "=== fit_correlation (singularity) ==="
echo "Input ROOT: $ROOT_FILE_REAL"
echo "Mainconf:   $MAINCONF_REAL"
echo "Histogram:  $HIST_NAME"
echo "Source dir: $FEMTOCALC_DIR"
echo "Output PDF: $OUTPUT_PDF"
echo "STAR tag:   $LIBRARY_TAG"
echo "===================================="

exec singularity exec \
  --pwd "$FEMTOCALC_DIR" \
  -B /gpfs:/gpfs \
  -B /star/nfs4/AFS:/star/nfs4/AFS \
  -B /home/starlib:/home/starlib \
  --env LD_LIBRARY_PATH="${LD_LIBRARY_PATH:-}" \
  "$SINGULARITY_IMAGE" \
  /bin/bash -lc "$CONTAINER_CMD"
