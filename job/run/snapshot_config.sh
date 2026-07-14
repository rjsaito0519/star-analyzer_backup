#!/bin/bash
# Write mainconf and all referenced config files into a single text file
# (configlog/config_anaName_jobid.txt) for reproducibility.
# Usage: ./snapshot_config.sh <anaName> <jobid> [mainconf_rel]

set -e
anaName="${1:?Usage: $0 <anaName> <jobid> [mainconf_rel]}"
jobid="${2:?Usage: $0 <anaName> <jobid> [mainconf_rel]}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
CONFIG_ROOT="$PROJECT_ROOT/config"
mainconf_rel="${3:-config/mainconf/main_${anaName}.yaml}"
mainconf_rel="${mainconf_rel#config/}"
mainconf_path="$CONFIG_ROOT/$mainconf_rel"
configlog_dir="$SCRIPT_DIR/configlog"
outfile="$configlog_dir/config_${anaName}_${jobid}.txt"

if [[ ! -f "$mainconf_path" ]]; then
  echo "ERROR: Main config not found: $mainconf_path" >&2
  exit 1
fi

mkdir -p "$configlog_dir"
: > "$outfile"

# Append one file to outfile: header "=== config/relpath ===" then contents
append_file() {
  local relpath="$1"
  local fullpath="$CONFIG_ROOT/$relpath"
  if [[ ! -f "$fullpath" ]]; then
    echo "WARNING: Skipping missing file: config/$relpath" >&2
    return
  fi
  echo "=== config/$relpath ===" >> "$outfile"
  cat "$fullpath" >> "$outfile"
  echo "" >> "$outfile"
}

# 1) Main config first
append_file "$mainconf_rel"

# 2) Paths referenced in mainconf (key: path, path is relative to config/)
while IFS= read -r relpath; do
  relpath=$(echo "$relpath" | sed 's/^[[:space:]]*//;s/[[:space:]]*$//')
  [[ -z "$relpath" ]] && continue
  append_file "$relpath"
done < <(grep -v '^#' "$mainconf_path" | grep -E '^[a-zA-Z0-9_]+:' | sed 's/^[^:]*:[[:space:]]*//')

echo "Wrote config snapshot: $outfile"
