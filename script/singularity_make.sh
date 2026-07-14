#!/bin/bash
# Build libStarAnaConfig.so and StMaker libraries inside batch-like singularity.
# Usage: ./script/singularity_make.sh MAINCONF [--no-clean] [make-args...]

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
PROJECT_ROOT_REAL="$(cd "$PROJECT_ROOT" && pwd -P)"
PYTHON="$(command -v python3 2>/dev/null || command -v python 2>/dev/null)"
SINGULARITY_IMAGE="/cvmfs/singularity.opensciencegrid.org/star-bnl/star-sw:latest"

MAINCONF="${1:-}"
RUN_CLEAN=1
MAKE_ARGS=()

if [[ -z "$MAINCONF" ]]; then
  echo "Usage: $0 MAINCONF [--no-clean] [make-args...]" >&2
  echo "  MAINCONF    main config (required, e.g. config/mainconf/main_auau3p85fxt_anaPhi.yaml)" >&2
  echo "  --no-clean  skip make clean before make" >&2
  echo "  make-args   optional extra arguments passed to make (default target: all)" >&2
  exit 1
fi

shift

while [[ $# -gt 0 ]]; do
  case "$1" in
    --no-clean)
      RUN_CLEAN=0
      shift
      ;;
    *)
      MAKE_ARGS+=("$1")
      shift
      ;;
  esac
done

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

resolve_star_host_sys() {
  local star_root="$1"
  local candidate
  local candidates=(sl73_x8664_gcc485 sl74_x8664_gcc485)
  if [[ -n "${STAR_HOST_SYS:-}" ]]; then
    candidates+=("$STAR_HOST_SYS")
  fi
  candidates+=(sl73_gcc485 sl74_gcc485)
  for candidate in "${candidates[@]}"; do
    if [[ -n "$candidate" && -d "$star_root/.$candidate" ]]; then
      printf '%s\n' "$candidate"
      return 0
    fi
  done
  return 1
}

resolve_root_bin() {
  local star_host_sys="$1"
  local root_arch_dir="linux-rhel7-x86"
  if [[ "$star_host_sys" == *x8664* ]]; then
    root_arch_dir="linux-rhel7-x86_64"
  fi

  local candidates=()
  shopt -s nullglob
  candidates=(/cvmfs/star.sdcc.bnl.gov/star-spack/spack/opt/spack/"$root_arch_dir"/gcc-4.8.5/root-*/bin)
  shopt -u nullglob

  if [[ -d "${candidates[0]:-}" ]]; then
    printf '%s\n' "${candidates[0]}"
  fi
}

resolve_cmake_bin() {
  local star_host_sys="$1"
  local root_arch_dir="linux-rhel7-x86"
  if [[ "$star_host_sys" == *x8664* ]]; then
    root_arch_dir="linux-rhel7-x86_64"
  fi

  local candidates=()
  shopt -s nullglob
  candidates=(/cvmfs/star.sdcc.bnl.gov/star-spack/spack/opt/spack/"$root_arch_dir"/gcc-4.8.5/cmake-*/bin/cmake)
  shopt -u nullglob

  if [[ -x "${candidates[0]:-}" ]]; then
    printf '%s\n' "${candidates[0]}"
    return 0
  fi

  command -v cmake 2>/dev/null || return 1
}

MAINCONF_REAL="$(resolve_from_project "$MAINCONF")"
if [[ ! -f "$MAINCONF_REAL" ]]; then
  echo "ERROR: mainconf not found: $MAINCONF_REAL" >&2
  exit 1
fi

LIBRARY_TAG=$(cd "$PROJECT_ROOT_REAL" && "$PYTHON" script/analysis_info_helper.py --library-tag --mainconf "$MAINCONF_REAL") || exit 1
LIBRARY_TAG="$(echo "$LIBRARY_TAG" | xargs)"
STAR="/star/nfs4/AFS/star/packages/$LIBRARY_TAG"

if command -v starver >/dev/null 2>&1; then
  # shellcheck source=/dev/null
  source "$SCRIPT_DIR/setup.sh" "$MAINCONF_REAL"
else
  if [[ ! -d "$STAR" ]]; then
    echo "ERROR: STAR package directory not found: $STAR" >&2
    echo "       Load a STAR environment (for example 'sl7') or run where /star/nfs4/AFS is visible." >&2
    exit 1
  fi
  if ! STAR_HOST_SYS="$(resolve_star_host_sys "$STAR")"; then
    echo "ERROR: could not resolve STAR_HOST_SYS under $STAR." >&2
    exit 1
  fi
  export STAR_HOST_SYS
fi

ROOT_BIN="$(resolve_root_bin "$STAR_HOST_SYS")"
CMAKE_BIN="$(resolve_cmake_bin "$STAR_HOST_SYS" || true)"
CMAKE_BIN_DIR=""
if [[ -n "$CMAKE_BIN" ]]; then
  CMAKE_BIN_DIR="$(dirname "$CMAKE_BIN")"
fi
BUILD_BITS="${BUILD_BITS:-64}"

echo "=== make (singularity) ==="
echo "Project: $PROJECT_ROOT_REAL"
echo "Config:  $MAINCONF"
echo "STAR:    $STAR"
echo "STAR_HOST_SYS: $STAR_HOST_SYS"
echo "BUILD_BITS: $BUILD_BITS"
if [[ -n "$CMAKE_BIN" ]]; then
  echo "cmake:   $CMAKE_BIN"
else
  echo "cmake:   not found (required after make clean for yaml-cpp)"
fi
echo "clean:   $([[ "$RUN_CLEAN" -eq 1 ]] && echo yes || echo no)"
if ((${#MAKE_ARGS[@]} > 0)); then
  echo "make:    ${MAKE_ARGS[*]}"
fi
echo "================================"

MAKE_COMMAND="make BUILD_BITS=$BUILD_BITS"
if ((${#MAKE_ARGS[@]} > 0)); then
  MAKE_COMMAND+=" ${MAKE_ARGS[*]}"
fi
if [[ "$RUN_CLEAN" -eq 1 ]]; then
  MAKE_COMMAND="make clean && $MAKE_COMMAND"
fi

if [[ "$RUN_CLEAN" -eq 1 && -z "$CMAKE_BIN" ]]; then
  echo "ERROR: cmake is required to rebuild src/third_party/yaml-cpp after make clean." >&2
  echo "       Re-run with --no-clean if yaml-cpp is already built, or use an environment with cmake." >&2
  exit 1
fi

CONTAINER_CMD=$(cat <<EOF
export STAR=$STAR
export STAR_HOST_SYS=$STAR_HOST_SYS
export STAR_LIB=\${STAR}/.\${STAR_HOST_SYS}/lib
export STAR_BIN=\${STAR}/.\${STAR_HOST_SYS}/bin
export PATH=\${STAR_BIN}:\${STAR}/mgr:\$PATH
export LD_LIBRARY_PATH=\${STAR_LIB}
if [[ -n "$ROOT_BIN" ]]; then
  export PATH=$ROOT_BIN:\$PATH
fi
if [[ -n "$CMAKE_BIN_DIR" ]]; then
  export PATH=$CMAKE_BIN_DIR:\$PATH
fi
cd "$PROJECT_ROOT_REAL"
$MAKE_COMMAND
EOF
)

exec singularity exec \
  --pwd "$PROJECT_ROOT_REAL" \
  -B /gpfs:/gpfs \
  -B /cvmfs:/cvmfs \
  -B /star/nfs4/AFS:/star/nfs4/AFS \
  /cvmfs/singularity.opensciencegrid.org/star-bnl/star-sw:latest \
  /bin/bash -lc "$CONTAINER_CMD"
