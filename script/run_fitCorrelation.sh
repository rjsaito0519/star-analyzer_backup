#!/bin/bash
# LL/KP femto CF fit helper.
# On AL9 (and when host root/ACLiC is unreliable), use the singularity wrapper.
#
# Usage:
#   ./script/run_fitCorrelation.sh <root_file> <mainconf_path> [hist_name]
#
# Legacy (SL7 only, ACLiC):
#   STAR_ANA_FIT_USE_ROOT=1 ./script/run_fitCorrelation.sh <root_file> [hist_name]

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

ROOT_FILE="${1:-}"
MAINCONF="${2:-}"
HIST_NAME="${3:-hCF}"

if [[ -z "$ROOT_FILE" ]]; then
  echo "Usage: $0 <root_file> <mainconf_path> [hist_name]" >&2
  echo "       (recommended on AL9: singularity wrapper is used by default)" >&2
  exit 1
fi

if [[ "${STAR_ANA_FIT_USE_ROOT:-}" == "1" ]]; then
  if [[ -z "$MAINCONF" ]]; then
    MAINCONF="${STAR_ANA_MAINCONF:-}"
    if [[ -z "$MAINCONF" && -f "$PROJECT_ROOT/.current_mainconf" ]]; then
      MAINCONF="$(<"$PROJECT_ROOT/.current_mainconf")"
    fi
  fi
  ROOT_CMD="${STAR_ANA_FIT_ROOT_CMD:-root}"
  if ! command -v "$ROOT_CMD" >/dev/null 2>&1; then
    echo "Error: $ROOT_CMD is not found in PATH." >&2
    exit 1
  fi
  if [[ ! -f "$ROOT_FILE" ]]; then
    echo "Error: file not found: $ROOT_FILE" >&2
    exit 1
  fi
  echo "=== fitCorrelation.C (host root / ACLiC) ==="
  echo "Input ROOT: $ROOT_FILE"
  echo "Histogram:  $HIST_NAME"
  echo "============================================"
  exec "$ROOT_CMD" -l -b -q "share/femtocalc/fitCorrelation.C+(\"$ROOT_FILE\",\"$HIST_NAME\")"
fi

if [[ -z "$MAINCONF" ]]; then
  MAINCONF="${STAR_ANA_MAINCONF:-}"
  if [[ -z "$MAINCONF" && -f "$PROJECT_ROOT/.current_mainconf" ]]; then
    MAINCONF="$(<"$PROJECT_ROOT/.current_mainconf")"
  fi
fi
if [[ -z "$MAINCONF" ]]; then
  echo "Error: mainconf is required. Pass as 2nd argument or set STAR_ANA_MAINCONF." >&2
  exit 1
fi

exec "$SCRIPT_DIR/singularity_run_fitCorrelation.sh" "$ROOT_FILE" "$MAINCONF" "$HIST_NAME"
