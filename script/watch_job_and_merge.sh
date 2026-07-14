#!/bin/bash
# Poll batch subjob ROOT output and run merge_root_files.csh when complete.
# Usage:
#   ./script/watch_job_and_merge.sh --runmeta job/run/runmeta/runmeta_<anaName>_<jobid>.json
#   ./script/watch_job_and_merge.sh --ana-name NAME --jobid HEX [--joblist PATH] [--mainconf PATH]

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
PYTHON="$(command -v python3 2>/dev/null || command -v python 2>/dev/null)"
JOB_RUN_DIR="$PROJECT_ROOT/job/run"
WATCHMERGE_DIR="$JOB_RUN_DIR/watchmerge"

POLL_SEC="${WATCH_MERGE_POLL_SEC:-300}"
TIMEOUT_SEC="${WATCH_MERGE_TIMEOUT_SEC:-$((72 * 3600))}"
ALLOW_PARTIAL=0
FORCE_MERGE=0
EXCLUDE_BAD_ROOTS=""

RUNMETA=""
ANA_NAME=""
JOBID=""
JOBLIST=""
MAINCONF=""

usage() {
  cat <<'EOF'
Usage: watch_job_and_merge.sh --runmeta PATH
   or: watch_job_and_merge.sh --ana-name NAME --jobid HEX [options]

Options:
  --runmeta PATH           Runmeta JSON from submit.sh (preferred)
  --ana-name NAME          Analysis name (manual mode)
  --jobid HEX              32-char SUMS job id
  --joblist PATH           Submitted joblist snapshot (manual mode)
  --mainconf PATH          Mainconf relative path (manual mode)
  --poll-sec N             Poll interval seconds (default 300, or WATCH_MERGE_POLL_SEC)
  --timeout-hours N        Give up after N hours (default 72, or WATCH_MERGE_TIMEOUT_SEC)
  --allow-partial          Merge when some subjob ROOT files exist (found > 0, found < N)
  --force-merge            Re-run merge even if *_merge.root already exists
  --exclude-bad-roots LIST Pass --exclude-list to merge_root_files.csh (after scan)
  -h, --help               Show this help
EOF
}

log_msg() {
  echo "[$(date -u +%Y-%m-%dT%H:%M:%SZ)] $*"
}

die() {
  log_msg "ERROR: $*"
  exit 1
}

read_runmeta_field() {
  local field="$1"
  "$PYTHON" - "$RUNMETA" "$field" <<'PY'
import json
import sys

with open(sys.argv[1], 'r') as handle:
    data = json.load(handle)

field = sys.argv[2]
if field == 'projectRoot':
    print(data.get('projectRoot', ''))
elif field == 'anaName':
    print(data.get('anaName', ''))
elif field == 'jobid':
    print(data.get('jobid', ''))
elif field == 'mainconf':
    print((data.get('submit') or {}).get('mainconf', ''))
elif field == 'joblistSnapshot':
    submit = data.get('submit') or {}
    path = (submit.get('joblistSnapshot') or {}).get('path')
    if not path:
        path = (submit.get('submittedXml') or {}).get('path')
    print(path or '')
PY
}

update_runmeta_status() {
  local status="$1"
  local expected="$2"
  local found="$3"
  local merge_root="${4:-}"
  local watch_log_path="${5:-}"
  local started_at="${6:-}"
  local finished_at="${7:-}"

  if [[ -z "$RUNMETA" || ! -f "$RUNMETA" ]]; then
    return 0
  fi

  STATUS="$status" EXPECTED="$expected" FOUND="$found" \
  MERGE_ROOT="$merge_root" WATCH_LOG_PATH="$watch_log_path" \
  STARTED="$started_at" FINISHED="$finished_at" RUNMETA_PATH="$RUNMETA" \
  "$PYTHON" - <<'PY' | (cd "$PROJECT_ROOT" && "$PYTHON" script/analysis_info_helper.py \
    --update-runmeta-postprocess "$RUNMETA")
import json
import os
import sys

payload = {
    "startedAt": os.environ["STARTED"],
    "finishedAt": os.environ["FINISHED"],
    "expectedSubjobs": int(os.environ["EXPECTED"]),
    "foundSubjobRoots": int(os.environ["FOUND"]),
    "mergeRoot": os.environ.get("MERGE_ROOT") or None,
    "mergeLog": os.environ.get("WATCH_LOG_PATH") or None,
    "status": os.environ["STATUS"],
}
print(json.dumps(payload))
PY
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --runmeta)
      RUNMETA="$2"
      shift 2
      ;;
    --ana-name)
      ANA_NAME="$2"
      shift 2
      ;;
    --jobid)
      JOBID="$2"
      shift 2
      ;;
    --joblist)
      JOBLIST="$2"
      shift 2
      ;;
    --mainconf)
      MAINCONF="$2"
      shift 2
      ;;
    --poll-sec)
      POLL_SEC="$2"
      shift 2
      ;;
    --timeout-hours)
      TIMEOUT_SEC="$(( $2 * 3600 ))"
      shift 2
      ;;
    --allow-partial)
      ALLOW_PARTIAL=1
      shift
      ;;
    --force-merge)
      FORCE_MERGE=1
      shift
      ;;
    --exclude-bad-roots)
      EXCLUDE_BAD_ROOTS="$2"
      shift 2
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      die "Unknown option: $1"
      ;;
  esac
done

if [[ -z "$PYTHON" ]]; then
  die "python3/python is required"
fi

if [[ -n "$RUNMETA" ]]; then
  if [[ ! -f "$RUNMETA" ]]; then
    die "runmeta not found: $RUNMETA"
  fi
  PROJECT_ROOT="$(read_runmeta_field projectRoot)"
  ANA_NAME="$(read_runmeta_field anaName)"
  JOBID="$(read_runmeta_field jobid)"
  MAINCONF="$(read_runmeta_field mainconf)"
  JOBLIST="$(read_runmeta_field joblistSnapshot)"
fi

if [[ -z "$ANA_NAME" || -z "$JOBID" ]]; then
  usage >&2
  die "require --runmeta or both --ana-name and --jobid"
fi

if [[ -z "$JOBLIST" || ! -f "$JOBLIST" ]]; then
  die "joblist snapshot not found: ${JOBLIST:-<unset>}"
fi

if [[ -z "$MAINCONF" ]]; then
  die "mainconf not found in runmeta or --mainconf"
fi

mkdir -p "$WATCHMERGE_DIR"
WATCH_LOG="$WATCHMERGE_DIR/watchmerge_${ANA_NAME}_${JOBID}.log"
PID_FILE="$WATCHMERGE_DIR/watchmerge_${ANA_NAME}_${JOBID}.pid"

if [[ -f "$PID_FILE" ]]; then
  old_pid="$(cat "$PID_FILE" 2>/dev/null || true)"
  if [[ -n "$old_pid" ]] && kill -0 "$old_pid" 2>/dev/null; then
    die "watch-merge already running (PID $old_pid, log: $WATCH_LOG)"
  fi
fi
echo $$ > "$PID_FILE"
trap 'rm -f "$PID_FILE"' EXIT

JOB_PREFIX="${ANA_NAME}${JOBID}"
STARTED_AT="$(date -u +%Y-%m-%dT%H:%M:%SZ)"

helper() {
  (cd "$PROJECT_ROOT" && "$PYTHON" script/analysis_info_helper.py "$@")
}

EXPECTED="$(helper --expected-subjobs-from-joblist "$JOBLIST" \
  --job-run-dir "$JOB_RUN_DIR" --job-prefix "$JOB_PREFIX")"
MERGE_SAMPLE="$(helper --merge-sample-from-joblist "$JOBLIST" --watch-merge-jobid "$JOBID")"
MERGE_OUTPUT="$(helper --merge-output-from-joblist "$JOBLIST" --watch-merge-jobid "$JOBID")"
ROOTFILE_DIR="$(helper --rootfile-dir-from-joblist "$JOBLIST")"
OUTPUT_STEM="$(helper --output-stem-from-joblist "$JOBLIST")"

exec >> "$WATCH_LOG" 2>&1
log_msg "watch-merge started anaName=$ANA_NAME jobid=$JOBID expectedSubjobs=$EXPECTED"
log_msg "pollSec=$POLL_SEC timeoutSec=$TIMEOUT_SEC allowPartial=$ALLOW_PARTIAL forceMerge=$FORCE_MERGE"
log_msg "rootfileDir=$ROOTFILE_DIR mergeSample=$MERGE_SAMPLE mergeOutput=$MERGE_OUTPUT"
if [[ -n "$EXCLUDE_BAD_ROOTS" ]]; then
  if [[ ! -f "$EXCLUDE_BAD_ROOTS" ]]; then
    die "exclude list not found: $EXCLUDE_BAD_ROOTS"
  fi
  excluded_count="$(awk 'NF{n++} END{print n+0}' "$EXCLUDE_BAD_ROOTS")"
  log_msg "exclude-bad-roots list=$EXCLUDE_BAD_ROOTS entries=$excluded_count"
fi

if [[ -f "$MERGE_OUTPUT" && "$FORCE_MERGE" -eq 0 ]]; then
  log_msg "merge output already exists; skipping merge: $MERGE_OUTPUT"
  update_runmeta_status "skipped_existing" "$EXPECTED" "$EXPECTED" "$MERGE_OUTPUT" "$WATCH_LOG" \
    "$STARTED_AT" "$(date -u +%Y-%m-%dT%H:%M:%SZ)"
  exit 0
fi

deadline=$(( $(date +%s) + TIMEOUT_SEC ))
stable_count=-1
stable_polls=0

while true; do
  now=$(date +%s)
  if (( now >= deadline )); then
    found="$(helper --rootfile-dir-from-joblist "$JOBLIST" --count-subjob-roots \
      --watch-merge-jobid "$JOBID")"
    log_msg "timeout after ${TIMEOUT_SEC}s (found $found/$EXPECTED subjob ROOT files)"
    update_runmeta_status "timeout" "$EXPECTED" "$found" "" "$WATCH_LOG" \
      "$STARTED_AT" "$(date -u +%Y-%m-%dT%H:%M:%SZ)"
    exit 1
  fi

  found="$(helper --rootfile-dir-from-joblist "$JOBLIST" --count-subjob-roots \
    --watch-merge-jobid "$JOBID")"
  log_msg "poll found=$found expected=$EXPECTED stablePolls=$stable_polls"

  if (( found >= EXPECTED )); then
    if (( found == stable_count )); then
      stable_polls=$((stable_polls + 1))
    else
      stable_count=$found
      stable_polls=1
    fi
    if (( stable_polls >= 2 )); then
      log_msg "all subjob ROOT files present ($found/$EXPECTED); proceeding to merge"
      break
    fi
  elif (( ALLOW_PARTIAL == 1 && found > 0 )); then
    if (( found == stable_count )); then
      stable_polls=$((stable_polls + 1))
    else
      stable_count=$found
      stable_polls=1
    fi
    if (( stable_polls >= 2 )); then
      log_msg "partial completion ($found/$EXPECTED) with --allow-partial; proceeding to merge"
      break
    fi
  else
    stable_count=-1
    stable_polls=0
  fi

  sleep "$POLL_SEC"
done

if [[ ! -f "$MERGE_SAMPLE" ]]; then
  first_match="$(find "$ROOTFILE_DIR" -maxdepth 1 -type f \
    -name "${OUTPUT_STEM}_${JOBID}_*.root" ! -name "${OUTPUT_STEM}_${JOBID}_merge.root" \
    | sort | head -1)"
  if [[ -n "$first_match" ]]; then
    MERGE_SAMPLE="$first_match"
    log_msg "sample _0.root missing; using $MERGE_SAMPLE"
  else
    log_msg "no subjob ROOT sample found under $ROOTFILE_DIR"
    update_runmeta_status "failed_no_sample" "$EXPECTED" "$found" "" "$WATCH_LOG" \
      "$STARTED_AT" "$(date -u +%Y-%m-%dT%H:%M:%SZ)"
    exit 1
  fi
fi

log_msg "running merge_root_files.csh on $MERGE_SAMPLE"
set +e
(
  cd "$PROJECT_ROOT"
  # shellcheck disable=SC1091
  source ./script/setup.sh "$MAINCONF"
  if [[ -n "$EXCLUDE_BAD_ROOTS" ]]; then
    ./script/merge_root_files.csh "--exclude-list=$EXCLUDE_BAD_ROOTS" "$MERGE_SAMPLE"
  else
    ./script/merge_root_files.csh "$MERGE_SAMPLE"
  fi
)
merge_rc=$?
set -e

FINISHED_AT="$(date -u +%Y-%m-%dT%H:%M:%SZ)"
if (( merge_rc != 0 )); then
  log_msg "merge_root_files.csh failed with exit code $merge_rc"
  update_runmeta_status "merge_failed" "$EXPECTED" "$found" "" "$WATCH_LOG" \
    "$STARTED_AT" "$FINISHED_AT"
  exit "$merge_rc"
fi

if [[ ! -f "$MERGE_OUTPUT" ]]; then
  log_msg "merge finished but output missing: $MERGE_OUTPUT"
  update_runmeta_status "failed_no_output" "$EXPECTED" "$found" "" "$WATCH_LOG" \
    "$STARTED_AT" "$FINISHED_AT"
  exit 1
fi

log_msg "merge completed: $MERGE_OUTPUT"
update_runmeta_status "ok" "$EXPECTED" "$found" "$MERGE_OUTPUT" "$WATCH_LOG" \
  "$STARTED_AT" "$FINISHED_AT"
exit 0
