#!/bin/bash
# Source from project root: source ./script/setup.sh MAINCONF_PATH
# For csh/tcsh use: source ./script/setup.csh MAINCONF_PATH
# MAINCONF_PATH is required (e.g. config/mainconf/main_auau19_anaLambda.yaml).

_setup_return() {
  return "$1" 2>/dev/null || exit "$1"
}

prepend_path() {
  local dir="$1"
  [[ -d "$dir" ]] || return 0
  case ":${PATH:-}:" in
    *":$dir:"*) ;;
    *) export PATH="$dir${PATH:+:$PATH}" ;;
  esac
}

prepend_ld_library_path() {
  local dir="$1"
  [[ -d "$dir" ]] || return 0
  case ":${LD_LIBRARY_PATH:-}:" in
    *":$dir:"*) ;;
    *) export LD_LIBRARY_PATH="$dir${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}" ;;
  esac
}

resolve_star_host_sys() {
  local candidate
  local candidates=()
  candidates+=(sl73_x8664_gcc485 sl74_x8664_gcc485)
  if [[ -n "${STAR_HOST_SYS:-}" ]]; then
    candidates+=("$STAR_HOST_SYS")
  fi
  candidates+=(sl73_gcc485 sl74_gcc485)
  for candidate in "${candidates[@]}"; do
    [[ -n "$candidate" && -d "$STAR/.$candidate" ]] && {
      printf '%s\n' "$candidate"
      return 0
    }
  done
  return 1
}

resolve_root_bin() {
  local root_arch_dir="linux-rhel7-x86"
  local candidates=()
  if [[ "${STAR_HOST_SYS:-}" == *x8664* ]]; then
    root_arch_dir="linux-rhel7-x86_64"
  fi

  shopt -s nullglob
  candidates=(/cvmfs/star.sdcc.bnl.gov/star-spack/spack/opt/spack/"$root_arch_dir"/gcc-4.8.5/root-*/bin)
  shopt -u nullglob

  [[ -d "${candidates[0]:-}" ]] && printf '%s\n' "${candidates[0]}"
}

if [[ "${BASH_SOURCE[0]:-}" == "$0" ]]; then
  echo "WARNING: executing ./script/setup.sh does not update your current shell." >&2
  echo "Use 'source ./script/setup.sh <mainconf>' (bash/zsh) or 'source ./script/setup.csh <mainconf>' (csh/tcsh)." >&2
fi

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
MAINCONF="${1:?Usage: source ./script/setup.sh MAINCONF_PATH (e.g. config/mainconf/main_auau19_anaLambda.yaml)}"
export STAR_ANA_MAINCONF="$MAINCONF"
echo "$MAINCONF" > "$PROJECT_ROOT/.current_mainconf"

LIBRARY_TAG=$(cd "$PROJECT_ROOT" && python script/analysis_info_helper.py --library-tag --mainconf "$MAINCONF") || _setup_return 1
LIBRARY_TAG=$(echo "$LIBRARY_TAG" | xargs)

if ! command -v starver >/dev/null 2>&1; then
  echo "ERROR: starver command not found. Load STAR environment first (for this project, run 'sl7' before setup/make)." >&2
  _setup_return 1
fi

starver "$LIBRARY_TAG" || _setup_return 1

STAR_TAG_DIR="/star/nfs4/AFS/star/packages/$LIBRARY_TAG"
if [[ -d "$STAR_TAG_DIR" ]]; then
  export STAR="$STAR_TAG_DIR"
fi

if ! resolved_star_host_sys=$(resolve_star_host_sys); then
  echo "ERROR: could not resolve STAR_HOST_SYS under ${STAR:-unset}." >&2
  _setup_return 1
fi

export STAR_HOST_SYS="$resolved_star_host_sys"
export STAR_BIN="$STAR/.$STAR_HOST_SYS/bin"
export STAR_LIB="$STAR/.$STAR_HOST_SYS/lib"

prepend_path "$STAR/mgr"
prepend_path "$STAR_BIN"

ROOT_BIN="$(resolve_root_bin)"
if [[ -n "$ROOT_BIN" ]]; then
  prepend_path "$ROOT_BIN"
fi

prepend_ld_library_path "$STAR_LIB"

echo "LIBRARY_TAG: $LIBRARY_TAG"
echo "STAR: $STAR"
echo "STAR_HOST_SYS: $STAR_HOST_SYS"

if command -v root-config >/dev/null 2>&1; then
  ROOT_CFLAGS="$(root-config --cflags 2>/dev/null || true)"
  if [[ "$STAR_HOST_SYS" == *x8664* && "$ROOT_CFLAGS" == *"-m32"* ]]; then
    echo "WARNING: root-config still points to 32-bit ROOT: $(command -v root-config)" >&2
  fi
fi
