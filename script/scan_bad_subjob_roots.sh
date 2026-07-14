#!/bin/bash
# Scan subjob ROOT files and emit exclusion list for merge_root_files.csh.
# Usage examples:
#   ./script/scan_bad_subjob_roots.sh --sample /path/to/ana_JOBID_0.root
#   ./script/scan_bad_subjob_roots.sh --dir /path/to/rootdir --prefix ana_JOBID

set -euo pipefail

SAMPLE=""
ROOT_DIR=""
PREFIX=""
OUTPUT=""
VERBOSE=0

usage() {
  cat <<'EOF'
Usage:
  scan_bad_subjob_roots.sh --sample PATH [--output FILE] [--verbose]
  scan_bad_subjob_roots.sh --dir DIR --prefix PREFIX [--output FILE] [--verbose]

Options:
  --sample PATH   Example subjob ROOT (e.g. anaName_JOBID_0.root)
  --dir DIR       ROOT directory (manual mode)
  --prefix PFX    Prefix before subjob index (e.g. anaName_JOBID)
  --output FILE   Exclusion-list path (default: /tmp/bad_subjob_roots_<prefix>.txt)
  --verbose       Print each rejected file with reason
  -h, --help      Show this help
EOF
}

abs_path() {
  python3 - "$1" <<'PY'
import os, sys
print(os.path.realpath(sys.argv[1]))
PY
}

derive_from_sample() {
  local sample="$1"
  local sample_abs base_no_ext
  sample_abs="$(abs_path "$sample")"
  ROOT_DIR="$(dirname "$sample_abs")"
  base_no_ext="$(basename "$sample_abs" .root)"
  if [[ "$base_no_ext" == *_merge ]]; then
    PREFIX="${base_no_ext%_merge}"
  else
    PREFIX="${base_no_ext%_*}"
  fi
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --sample)
      SAMPLE="$2"
      shift 2
      ;;
    --dir)
      ROOT_DIR="$2"
      shift 2
      ;;
    --prefix)
      PREFIX="$2"
      shift 2
      ;;
    --output)
      OUTPUT="$2"
      shift 2
      ;;
    --verbose)
      VERBOSE=1
      shift
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      echo "ERROR: Unknown option: $1" >&2
      usage >&2
      exit 1
      ;;
  esac
done

if [[ -n "$SAMPLE" ]]; then
  [[ -f "$SAMPLE" ]] || { echo "ERROR: sample not found: $SAMPLE" >&2; exit 1; }
  derive_from_sample "$SAMPLE"
fi

if [[ -z "$ROOT_DIR" || -z "$PREFIX" ]]; then
  echo "ERROR: require --sample, or both --dir and --prefix" >&2
  usage >&2
  exit 1
fi

ROOT_DIR="$(abs_path "$ROOT_DIR")"
[[ -d "$ROOT_DIR" ]] || { echo "ERROR: root dir not found: $ROOT_DIR" >&2; exit 1; }

if [[ -z "$OUTPUT" ]]; then
  safe_prefix="$(echo "$PREFIX" | tr '/ ' '__')"
  OUTPUT="/tmp/bad_subjob_roots_${safe_prefix}.txt"
fi
OUTPUT="$(abs_path "$OUTPUT")"
mkdir -p "$(dirname "$OUTPUT")"

pattern="${PREFIX}_*.root"
merge_name="${PREFIX}_merge.root"
tmp_candidates="$(mktemp)"
tmp_reasons="$(mktemp)"
trap 'rm -f "$tmp_candidates" "$tmp_reasons"' EXIT

find "$ROOT_DIR" -maxdepth 1 -type f -name "$pattern" ! -name "$merge_name" | sort > "$tmp_candidates"
total="$(wc -l < "$tmp_candidates" | awk '{print $1}')"
if [[ -z "$total" || "$total" -lt 1 ]]; then
  echo "ERROR: no candidate files found: $ROOT_DIR/$pattern" >&2
  exit 1
fi

have_rootls=0
if command -v rootls >/dev/null 2>&1; then
  have_rootls=1
fi

bad=0
good=0
: > "$tmp_reasons"

while IFS= read -r f; do
  [[ -n "$f" ]] || continue
  reason=""

  size="$(stat -c '%s' "$f" 2>/dev/null || echo 0)"
  if [[ "$size" -le 0 ]]; then
    reason="zero_size"
  elif [[ "$have_rootls" -eq 1 ]]; then
    if ! rootls -1 "$f" >/dev/null 2>&1; then
      reason="rootls_failed"
    else
      nkeys="$(rootls -1 "$f" 2>/dev/null | wc -l | awk '{print $1}')"
      if [[ "${nkeys:-0}" -eq 0 ]]; then
        reason="no_keys"
      fi
    fi
  fi

  if [[ -n "$reason" ]]; then
    bad=$((bad + 1))
    printf '%s\t%s\n' "$f" "$reason" >> "$tmp_reasons"
    if [[ "$VERBOSE" -eq 1 ]]; then
      echo "BAD: $f ($reason)"
    fi
  else
    good=$((good + 1))
  fi
done < "$tmp_candidates"

cut -f1 "$tmp_reasons" > "$OUTPUT"

echo "scan_bad_subjob_roots.sh summary"
echo "  dir: $ROOT_DIR"
echo "  prefix: $PREFIX"
echo "  total: $total"
echo "  good: $good"
echo "  bad: $bad"
echo "  output: $OUTPUT"
if [[ "$bad" -gt 0 ]]; then
  echo "  reason file: ${OUTPUT}.reasons.tsv"
  cp -f "$tmp_reasons" "${OUTPUT}.reasons.tsv"
fi

exit 0
