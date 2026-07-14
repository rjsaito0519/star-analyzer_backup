#!/bin/bash
# Submit from job/run/. Replaces __PROJECT_ROOT__ in the XML with the project root.
# Usage: cd job/run && ./submit.sh [joblist.xml]
# Example: ./submit.sh ../joblist/joblist_auau19_anaLambda_temp.xml

set -e
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

"$SCRIPT_DIR/setup_symlinks.sh"

PROJECT_ROOT="$(cd .. && cd .. && pwd)"
REBUILD_IF_NEEDED=0
WATCH_MERGE=0
WATCH_MERGE_FOREGROUND=0
TEMPLATE=""
while [[ $# -gt 0 ]]; do
  case "$1" in
    --rebuild-if-needed)
      REBUILD_IF_NEEDED=1
      shift
      ;;
    --watch-merge)
      WATCH_MERGE=1
      shift
      ;;
    --watch-merge-foreground)
      WATCH_MERGE=1
      WATCH_MERGE_FOREGROUND=1
      shift
      ;;
    -h|--help)
      cat <<'EOF'
Usage: ./submit.sh [--rebuild-if-needed] [--watch-merge] [joblist.xml]

Options:
  --rebuild-if-needed        Run setup+make once if preflight fails.
  --watch-merge              After submit, poll subjob ROOT output and run merge_root_files.csh in background.
  --watch-merge-foreground   Same as --watch-merge but run watcher in foreground (debug).
  -h, --help                 Show this help.

Example:
  ./submit.sh --watch-merge ../joblist/joblist_auau3p85fxt_anaFemtoPhiProton.xml
EOF
      exit 0
      ;;
    -*)
      echo "ERROR: Unknown option: $1" >&2
      exit 1
      ;;
    *)
      TEMPLATE="$1"
      shift
      ;;
  esac
done
TEMPLATE="${TEMPLATE:-../joblist/joblist_auau19_anaLambda_temp.xml}"
OUTPUT="$(basename "$TEMPLATE")"

if [[ ! -f "$TEMPLATE" ]]; then
  echo "ERROR: Template not found: $TEMPLATE" >&2
  exit 1
fi

TEMPLATE_ABS="$(cd "$(dirname "$TEMPLATE")" && pwd)/$(basename "$TEMPLATE")"

extract_elf_class() {
  local target="$1"
  local info
  info=$(file -L "$target" 2>/dev/null || true)
  if [[ "$info" == *"ELF 64-bit"* ]]; then
    echo "ELF64"
    return 0
  fi
  if [[ "$info" == *"ELF 32-bit"* ]]; then
    echo "ELF32"
    return 0
  fi
  return 1
}

run_preflight() {
  local ana_name="$1"
  local mainconf_rel="$2"
  local -a required_libs
  local mainconf_path="$PROJECT_ROOT/$mainconf_rel"
  local root4star_bin root4star_class expected_class lib class
  local setup_probe setup_star_host setup_root_libs
  local runtime_probe runtime_missing
  local library_tag
  local -a details

  if [[ ! -f "$mainconf_path" ]]; then
    details+=("missing mainconf: $mainconf_rel")
  fi

  if [[ ! -f "$PROJECT_ROOT/lib/libStarAnaConfig.so" ]]; then
    details+=("missing lib/libStarAnaConfig.so")
  fi

  required_libs=("$PROJECT_ROOT/lib/"*.so)
  if [[ ! -e "${required_libs[0]}" ]]; then
    details+=("no shared libraries found under lib/")
  fi

  library_tag=$(cd "$PROJECT_ROOT" && python script/analysis_info_helper.py --library-tag --mainconf "$mainconf_rel" 2>/dev/null | xargs || true)
  if [[ -z "$library_tag" ]]; then
    details+=("cannot resolve libraryTag via analysis_info_helper.py from $mainconf_rel")
  else
    # Determine expected class from runtime context (STAR_HOST_SYS / root-config), not from libraryTag string.
    setup_probe=$(bash -lc "cd \"$PROJECT_ROOT\" && source ./script/setup.sh \"$mainconf_rel\" >/dev/null 2>&1; echo \"STAR_HOST_SYS=\${STAR_HOST_SYS:-}\"; echo \"ROOT_LIBS=\$(root-config --libs 2>/dev/null || true)\"; command -v root4star 2>/dev/null || true" 2>/dev/null || true)
    setup_star_host=$(printf "%s\n" "$setup_probe" | sed -n 's/^STAR_HOST_SYS=//p' | head -1)
    setup_root_libs=$(printf "%s\n" "$setup_probe" | sed -n 's/^ROOT_LIBS=//p' | head -1)
    root4star_bin=$(printf "%s\n" "$setup_probe" | awk '/\/root4star$/ {print; exit}')

    if [[ "$setup_star_host" == *"x8664"* || "$setup_root_libs" == *"x86_64"* ]]; then
      expected_class="ELF64"
    elif [[ -n "$setup_star_host" || -n "$setup_root_libs" ]]; then
      expected_class="ELF32"
    fi

    if [[ -z "$expected_class" && -n "$root4star_bin" ]]; then
      if root4star_class=$(extract_elf_class "$root4star_bin"); then
        expected_class="$root4star_class"
      fi
    fi

    if [[ -z "$expected_class" ]]; then
      details+=("cannot determine expected ELF class from setup runtime (STAR_HOST_SYS/root-config)")
    else
      echo "Preflight info: anaName=$ana_name mainconf=$mainconf_rel libraryTag=$library_tag STAR_HOST_SYS=${setup_star_host:-unknown} expectedClass=$expected_class"
    fi
  fi

  for lib in "${required_libs[@]}"; do
    [[ -f "$lib" ]] || continue
    if ! class=$(extract_elf_class "$lib"); then
      details+=("cannot determine ELF class: ${lib#$PROJECT_ROOT/}")
      continue
    fi
    if [[ -z "$expected_class" ]]; then
      expected_class="$class"
      continue
    fi
    if [[ "$class" != "$expected_class" ]]; then
      details+=("ELF class mismatch: ${lib#$PROJECT_ROOT/} is $class, expected $expected_class")
    fi
  done

  # Runtime linker sanity check (fail-fast): verify root4star dependencies resolve
  # in the same singularity context used by current joblists.
  if command -v singularity >/dev/null 2>&1; then
    runtime_probe=$(bash -lc "cd \"$PROJECT_ROOT\" && source ./script/setup.sh \"$mainconf_rel\" >/dev/null 2>&1; singularity exec -B /star/nfs4/AFS:/star/nfs4/AFS --env LD_LIBRARY_PATH=\"\$LD_LIBRARY_PATH\" /cvmfs/singularity.opensciencegrid.org/star-bnl/star-sw:latest /usr/bin/ldd /star/nfs4/AFS/star/packages/SL24y/.sl73_x8664_gcc485/bin/root4star 2>/dev/null || true" 2>/dev/null || true)
    runtime_missing=$(printf "%s\n" "$runtime_probe" | awk '/not found/ {print}')
    if [[ -n "$runtime_missing" ]]; then
      details+=("runtime linker unresolved in singularity context: $(printf "%s" "$runtime_missing" | tr '\n' '; ')")
    fi
  else
    details+=("singularity command is not available on submit host; cannot verify runtime linker sanity")
  fi

  if [[ ${#details[@]} -gt 0 ]]; then
    echo "Preflight failed (primary cause likely ABI mismatch; scheduler/rootfile errors are secondary):" >&2
    for item in "${details[@]}"; do
      echo "  - $item" >&2
    done
    return 1
  fi
  return 0
}

# anaName from joblist filename (e.g. joblist_auau3p85fxt_anaPhi.xml -> auau3p85fxt_anaPhi)
anaName="${OUTPUT#joblist_}"
anaName="${anaName%.xml}"
if [[ -z "$anaName" || "$anaName" == "$OUTPUT" ]]; then
  echo "ERROR: Could not derive anaName from joblist filename: $OUTPUT" >&2
  echo "Expected filename pattern: joblist_<anaName>.xml" >&2
  exit 1
fi

mainconf_rel=$(cd "$PROJECT_ROOT" && python script/analysis_info_helper.py --mainconf-from-joblist "$TEMPLATE_ABS" 2>/dev/null | xargs || true)
if [[ -z "$mainconf_rel" ]]; then
  echo "ERROR: Could not extract embedded mainconf from joblist: $TEMPLATE" >&2
  exit 1
fi

if ! run_preflight "$anaName" "$mainconf_rel"; then
  if [[ "$REBUILD_IF_NEEDED" -eq 1 ]]; then
    echo "Preflight failed; trying rebuild path: source ./script/setup.sh $mainconf_rel && make clean && make"
    bash -lc "cd \"$PROJECT_ROOT\" && source ./script/setup.sh \"$mainconf_rel\" && make clean && make"
    run_preflight "$anaName" "$mainconf_rel"
  else
    echo "Hint: run './submit.sh --rebuild-if-needed $TEMPLATE' or rebuild manually before submit." >&2
    exit 1
  fi
fi

echo "Ensuring batch output directories from joblist..."
if ! (cd "$PROJECT_ROOT" && python script/analysis_info_helper.py --ensure-batch-dirs "$TEMPLATE_ABS"); then
  echo "ERROR: failed to create or verify batch log/err/rootfile directories." >&2
  exit 1
fi

sed "s|__PROJECT_ROOT__|$PROJECT_ROOT|g" "$TEMPLATE" > "$OUTPUT"
echo "Submitting with PROJECT_ROOT=$PROJECT_ROOT"

# Capture star-submit output to get jobid for configlog
submit_out=$(mktemp)
trap "rm -f '$submit_out'" EXIT
star-submit "$OUTPUT" 2>&1 | tee "$submit_out"
submit_rc=${PIPESTATUS[0]}

# Extract jobid: from "Wrote scheduling report to : .../anaNameJOBID.report"
jobid=""
if report_line=$(grep "Wrote scheduling report to" "$submit_out" 2>/dev/null); then
  report_path=$(echo "$report_line" | sed 's/.*: *//' | tr -d '\r' | sed 's/^[[:space:]]*//;s/[[:space:]]*$//')
  if [[ -n "$report_path" ]]; then
    report_basename=$(basename "$report_path" .report)
    jobid="${report_basename#$anaName}"
  fi
fi
# Fallback: 32-char hex from "Writting files for process HEX_XX..done."
if [[ -z "$jobid" ]]; then
  jobid=$(grep -oE '[0-9A-Fa-f]{32}' "$submit_out" 2>/dev/null | head -1)
fi

if [[ -n "$jobid" ]]; then
  "$SCRIPT_DIR/snapshot_config.sh" "$anaName" "$jobid" "$mainconf_rel" || true
  "$SCRIPT_DIR/snapshot_joblist.sh" "$anaName" "$jobid" "$OUTPUT" || true
  "$SCRIPT_DIR/snapshot_runmeta.sh" "$anaName" "$jobid" "$OUTPUT" "$mainconf_rel" "$submit_out" || true

  if [[ "$WATCH_MERGE" -eq 1 ]]; then
    RUNMETA_JSON="$SCRIPT_DIR/runmeta/runmeta_${anaName}_${jobid}.json"
    WATCHMERGE_DIR="$SCRIPT_DIR/watchmerge"
    mkdir -p "$WATCHMERGE_DIR"
    WATCH_LOG="$WATCHMERGE_DIR/watchmerge_${anaName}_${jobid}.log"
    WATCH_CMD=("$PROJECT_ROOT/script/watch_job_and_merge.sh" --runmeta "$RUNMETA_JSON")
    if [[ "$WATCH_MERGE_FOREGROUND" -eq 1 ]]; then
      echo "Starting watch-merge in foreground (log: $WATCH_LOG)"
      "${WATCH_CMD[@]}"
    else
      nohup "${WATCH_CMD[@]}" >> "$WATCH_LOG" 2>&1 &
      watch_pid=$!
      echo "watch-merge started (PID $watch_pid, log: $WATCH_LOG)"
    fi
  fi
else
  echo "WARNING: Could not extract jobid; skipping configlog, joblistlog, and runmeta." >&2
  if [[ "$WATCH_MERGE" -eq 1 ]]; then
    echo "WARNING: --watch-merge requires a jobid; watcher not started." >&2
  fi
fi

exit "$submit_rc"
