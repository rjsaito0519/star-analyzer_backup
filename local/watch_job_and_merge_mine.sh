#!/bin/bash
# Personal watch-merge: Condor delta progress, scratch hadd/PDF, Discord overwrite,
# then final merge_root_files.csh. Does not modify official script/watch_job_and_merge.sh.
#
# Usage:
#   ./local/watch_job_and_merge_mine.sh --runmeta job/run/runmeta/runmeta_<anaName>_<jobid>.json
#   ./local/watch_job_and_merge_mine.sh --ana-name NAME --jobid HEX [--joblist PATH] [--mainconf PATH]

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
PYTHON="$(command -v python3 2>/dev/null || command -v python 2>/dev/null)"
JOB_RUN_DIR="$PROJECT_ROOT/job/run"

POLL_SEC="${WATCH_MERGE_POLL_SEC:-300}"
PROGRESS_SEC="${WATCH_MERGE_PROGRESS_SEC:-3600}"
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
Usage: watch_job_and_merge_mine.sh --runmeta PATH
   or: watch_job_and_merge_mine.sh --ana-name NAME --jobid HEX [options]

Personal watcher (mysubmit). Defaults: poll Condor+ROOT, hourly scratch hadd/PDF,
Discord message overwrite, then final merge when this JOBID is done.

Options:
  --runmeta PATH           Runmeta JSON from submit.sh (preferred)
  --ana-name NAME          Analysis name (manual mode)
  --jobid HEX              32-char SUMS job id
  --joblist PATH           Submitted joblist snapshot (manual mode)
  --mainconf PATH          Mainconf relative path (manual mode)
  --poll-sec N             Poll interval seconds (default 300)
  --progress-sec N         Progress hadd/PDF interval (default 3600)
  --timeout-hours N        Give up after N hours (default 72)
  --allow-partial          Final merge when some ROOT files exist (found > 0)
  --force-merge            Re-run merge even if *_merge.root already exists
  --exclude-bad-roots LIST Pass --exclude-list to merge_root_files.csh
  -h, --help               Show this help

Volatile root is always $PROJECT_ROOT/scratch unless STAR_ANALYZER_SCRATCH
points at a non-/tmp path. Env SCRATCH is ignored (SUMS uses that name).
Layout:
  scratch/<anaName>/<jobid>/{watch,progress,log,err}
EOF
}

log_msg() {
  echo "[$(date -u +%Y-%m-%dT%H:%M:%SZ)] $*"
}

die() {
  log_msg "ERROR: $*"
  exit 1
}

require_scratch() {
  local default_root="$PROJECT_ROOT/scratch"
  local root="$default_root"
  if [[ -n "${STAR_ANALYZER_SCRATCH:-}" ]]; then
    case "$STAR_ANALYZER_SCRATCH" in
      /tmp|/tmp/*)
        log_msg "WARN: ignoring STAR_ANALYZER_SCRATCH=$STAR_ANALYZER_SCRATCH (/tmp is not cluster-visible); using $default_root"
        ;;
      *)
        root="$STAR_ANALYZER_SCRATCH"
        ;;
    esac
  fi
  mkdir -p "$root" || die "cannot create volatile root=$root"
  STAR_ANALYZER_SCRATCH="$(cd "$root" && pwd)"
  export STAR_ANALYZER_SCRATCH
}

scratch_base_for() {
  local ana="$1" jid="$2"
  echo "${STAR_ANALYZER_SCRATCH}/${ana}/${jid}"
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
  local extra_json="${8:-{}}"

  if [[ -z "$RUNMETA" || ! -f "$RUNMETA" ]]; then
    return 0
  fi

  STATUS="$status" EXPECTED="$expected" FOUND="$found" \
  MERGE_ROOT="$merge_root" WATCH_LOG_PATH="$watch_log_path" \
  STARTED="$started_at" FINISHED="$finished_at" EXTRA_JSON="$extra_json" \
  "$PYTHON" - <<'PY' | (cd "$PROJECT_ROOT" && "$PYTHON" script/analysis_info_helper.py \
    --update-runmeta-postprocess "$RUNMETA")
import json
import os

extra = {}
raw = os.environ.get("EXTRA_JSON") or "{}"
try:
    extra = json.loads(raw)
except Exception:
    extra = {}

payload = {
    "startedAt": os.environ["STARTED"],
    "finishedAt": os.environ["FINISHED"],
    "expectedSubjobs": int(os.environ["EXPECTED"]),
    "foundSubjobRoots": int(os.environ["FOUND"]),
    "mergeRoot": os.environ.get("MERGE_ROOT") or None,
    "mergeLog": os.environ.get("WATCH_LOG_PATH") or None,
    "status": os.environ["STATUS"],
}
payload.update(extra)
print(json.dumps(payload))
PY
}

discord_webhook_url() {
  if [[ ! -f "$PROJECT_ROOT/.discord_webhook" ]]; then
    return 1
  fi
  local url
  url=$(tr -d '[:space:]' < "$PROJECT_ROOT/.discord_webhook" || true)
  [[ -n "$url" ]] || return 1
  printf '%s' "$url"
}

# Upsert one Discord message per jobid (wait=true create, then PATCH). Soft-fail always.
# Always use curl (not urllib): Discord Cloudflare blocks default Python-urllib UA with 403/1010.
discord_upsert() {
  local content="$1"
  local attach_path="${2:-}"
  local url mid resp code
  url="$(discord_webhook_url)" || {
    log_msg "Discord webhook missing; skip notify"
    return 0
  }

  mid=""
  if [[ -f "$DISCORD_MSG_FILE" ]]; then
    mid=$(tr -d '[:space:]' < "$DISCORD_MSG_FILE" || true)
  fi

  RESP_FILE="$(mktemp)"
  CODE_FILE="$(mktemp)"
  CONTENT="$content" URL="$url" MSG_ID="$mid" ATTACH="$attach_path" \
  RESP_FILE="$RESP_FILE" CODE_FILE="$CODE_FILE" DISCORD_MSG_FILE="$DISCORD_MSG_FILE" \
  "$PYTHON" - <<'PY'
import json
import os
import subprocess

content = os.environ["CONTENT"]
url = os.environ["URL"].rstrip("/")
msg_id = (os.environ.get("MSG_ID") or "").strip()
attach = (os.environ.get("ATTACH") or "").strip()
resp_path = os.environ["RESP_FILE"]
code_path = os.environ["CODE_FILE"]
msg_file = os.environ["DISCORD_MSG_FILE"]
ua = "star-analyzer-mysubmit/1.0"

def write_result(code, body):
    with open(code_path, "w") as handle:
        handle.write(str(code))
    with open(resp_path, "w") as handle:
        handle.write(body)

def run_curl(cmd):
    proc = subprocess.run(cmd, capture_output=True, text=True)
    out = (proc.stdout or "") + (proc.stderr or "")
    lines = out.strip().splitlines()
    code = lines[-1] if lines else "0"
    body = "\n".join(lines[:-1]) if len(lines) > 1 else ""
    return code, body

def maybe_save_id(code, body, had_msg_id):
    if not code.startswith("2") or had_msg_id:
        return
    try:
        data = json.loads(body)
        new_id = data.get("id")
        if new_id:
            with open(msg_file, "w") as handle:
                handle.write(str(new_id) + "\n")
    except Exception:
        pass

payload = {"content": content}
payload_json = json.dumps(payload)

try:
    if attach and os.path.isfile(attach):
        if msg_id:
            cmd = [
                "curl", "-sS", "-A", ua, "-X", "PATCH",
                "%s/messages/%s" % (url, msg_id),
                "-F", "payload_json=%s" % payload_json,
                "-F", "file=@%s" % attach,
                "-w", "\n%{http_code}",
            ]
        else:
            cmd = [
                "curl", "-sS", "-A", ua, "-X", "POST",
                url + "?wait=true",
                "-F", "payload_json=%s" % payload_json,
                "-F", "file=@%s" % attach,
                "-w", "\n%{http_code}",
            ]
        code, body = run_curl(cmd)
        write_result(code, body)
        maybe_save_id(code, body, bool(msg_id))
        if code.startswith("2"):
            raise SystemExit(0)
        # Attach failed: fall through to text-only curl.

    if msg_id:
        cmd = [
            "curl", "-sS", "-A", ua, "-X", "PATCH",
            "%s/messages/%s" % (url, msg_id),
            "-H", "Content-Type: application/json",
            "-d", payload_json,
            "-w", "\n%{http_code}",
        ]
    else:
        cmd = [
            "curl", "-sS", "-A", ua, "-X", "POST",
            url + "?wait=true",
            "-H", "Content-Type: application/json",
            "-d", payload_json,
            "-w", "\n%{http_code}",
        ]
    code, body = run_curl(cmd)
    write_result(code, body)
    maybe_save_id(code, body, bool(msg_id))
except SystemExit:
    raise
except Exception as exc:
    write_result("0", str(exc))
PY
  code=$(cat "$CODE_FILE" 2>/dev/null || echo 0)
  resp=$(cat "$RESP_FILE" 2>/dev/null || true)
  rm -f "$RESP_FILE" "$CODE_FILE"
  if [[ "$code" == 2* ]]; then
    log_msg "Discord upsert ok (http $code)"
  else
    log_msg "WARNING: Discord upsert failed (http $code): ${resp:0:200}"
  fi
  return 0
}

resolve_checkhist_script() {
  local suffix
  suffix=$(echo "${ANA_NAME#*_}" | sed 's/^[a-z]/\U&/')
  local cand="$PROJECT_ROOT/script/singularity_checkHist${suffix}.sh"
  if [[ -x "$cand" ]]; then
    echo "$cand"
    return 0
  fi
  return 1
}

# Print space-separated stable SUMS process indices for this JOBID still in Condor queue.
# Exit 0 always; sets CONDOR_QUERY_OK=1|0 via stdout marker line "OK|FAIL count ids..."
query_condor_procs() {
  if ! command -v condor_q >/dev/null 2>&1; then
    echo "FAIL"
    return 0
  fi
  "$PYTHON" - "$JOBID" <<'PY'
import re
import subprocess
import sys

jobid = sys.argv[1]
pat = re.compile(r'(?:^|[^0-9A-Fa-f])' + re.escape(jobid) + r'_([0-9]+)(?:[^0-9]|$)')
try:
    out = subprocess.check_output(
        ["condor_q", "-af", "Args", "Out", "Cmd", "Err"],
        stderr=subprocess.DEVNULL,
        text=True,
    )
except Exception:
    print("FAIL")
    sys.exit(0)

found = set()
for line in out.splitlines():
    if jobid not in line:
        continue
    for match in pat.finditer(line):
        found.add(int(match.group(1)))

ids = sorted(found)
print("OK " + " ".join(str(i) for i in ids))
PY
}

load_prev_condor_set() {
  PREV_CONDOR_IDS=()
  if [[ -f "$CONDOR_SET_FILE" ]]; then
    mapfile -t PREV_CONDOR_IDS < "$CONDOR_SET_FILE" || true
  fi
}

save_condor_set() {
  local -a ids=("$@")
  : > "$CONDOR_SET_FILE"
  local id
  for id in "${ids[@]}"; do
    [[ -n "$id" ]] || continue
    printf '%s\n' "$id" >> "$CONDOR_SET_FILE"
  done
}

set_diff_left() {
  # newly_left = prev - curr
  local -a prev=("$@")
  local -a left=()
  local p c hit
  for p in "${prev[@]}"; do
    [[ -n "$p" ]] || continue
    hit=0
    for c in "${CURR_CONDOR_IDS[@]+"${CURR_CONDOR_IDS[@]}"}"; do
      if [[ "$p" == "$c" ]]; then
        hit=1
        break
      fi
    done
    if [[ "$hit" -eq 0 ]]; then
      left+=("$p")
    fi
  done
  NEWLY_LEFT=("${left[@]+"${left[@]}"}")
}

list_stable_subjob_roots() {
  # Prints paths of subjob ROOT files whose size was unchanged vs SIZE_CACHE_FILE.
  "$PYTHON" - "$ROOTFILE_DIR" "$OUTPUT_STEM" "$JOBID" "$SIZE_CACHE_FILE" <<'PY'
import os
import sys

rootfile_dir, stem, jobid, cache_path = sys.argv[1:5]
prefix = "{}_{}_".format(stem, jobid)
merge_name = "{}_{}_merge.root".format(stem, jobid)

prev = {}
if os.path.isfile(cache_path):
    with open(cache_path, "r") as handle:
        for line in handle:
            line = line.strip()
            if not line or "\t" not in line:
                continue
            path, size = line.split("\t", 1)
            try:
                prev[path] = int(size)
            except ValueError:
                pass

curr = {}
stable = []
if os.path.isdir(rootfile_dir):
    for name in os.listdir(rootfile_dir):
        if not name.endswith(".root") or name == merge_name:
            continue
        if not name.startswith(prefix):
            continue
        path = os.path.join(rootfile_dir, name)
        try:
            size = os.path.getsize(path)
        except OSError:
            continue
        curr[path] = size
        if path in prev and prev[path] == size and size > 0:
            stable.append(path)

with open(cache_path, "w") as handle:
    for path, size in sorted(curr.items()):
        handle.write("{}\t{}\n".format(path, size))

for path in sorted(stable):
    print(path)
PY
}

run_progress_hadd() {
  local list_file="$PROGRESS_DIR/stable_roots.txt"
  local out_root="$PROGRESS_DIR/partial_merge.root"
  local -a files=()
  mapfile -t files < <(list_stable_subjob_roots || true)
  if [[ "${#files[@]}" -lt 1 ]]; then
    log_msg "progress hadd: no stable subjob ROOT files yet"
    return 1
  fi
  printf '%s\n' "${files[@]}" > "$list_file"
  log_msg "progress hadd: ${#files[@]} stable ROOT(s) -> $out_root"
  if ! command -v hadd >/dev/null 2>&1; then
    log_msg "WARNING: hadd not found; skip progress merge"
    return 1
  fi
  set +e
  LIST_FILE="$list_file" OUT_ROOT="$out_root" PROGRESS_DIR="$PROGRESS_DIR" \
  "$PYTHON" - <<'PY'
import os
import subprocess
import sys

list_file = os.environ["LIST_FILE"]
out_root = os.environ["OUT_ROOT"]
progress_dir = os.environ["PROGRESS_DIR"]
tmp = os.path.join(progress_dir, "_hadd_acc.root")
swap = tmp + ".swap"

paths = []
with open(list_file, "r") as handle:
    for line in handle:
        line = line.strip()
        if line:
            paths.append(line)

if not paths:
    sys.exit(1)

for path in (tmp, swap, out_root):
    if os.path.isfile(path):
        os.remove(path)

batch_size = 80
acc = None
for i in range(0, len(paths), batch_size):
    batch = paths[i:i + batch_size]
    if acc is None:
        cmd = ["hadd", "-f", tmp] + batch
    else:
        cmd = ["hadd", "-f", swap, acc] + batch
    proc = subprocess.run(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    if proc.returncode != 0:
        sys.exit(proc.returncode)
    if acc is None:
        acc = tmp
    else:
        os.replace(swap, tmp)
        acc = tmp

os.replace(tmp, out_root)
PY
  local rc=$?
  set -e
  if (( rc != 0 )) || [[ ! -f "$out_root" ]]; then
    log_msg "WARNING: progress hadd failed (rc=$rc)"
    return 1
  fi
  return 0
}

run_checkhist_pdf() {
  local root_in="$1"
  local dest_pdf="$2"
  local script
  script="$(resolve_checkhist_script)" || {
    log_msg "No checkHist script for $ANA_NAME; skip PDF"
    return 1
  }
  log_msg "Running checkHist: $script on $root_in"
  set +e
  "$script" "$root_in" "$MAINCONF"
  local rc=$?
  set -e
  if (( rc != 0 )); then
    log_msg "WARNING: checkHist failed rc=$rc"
    return 1
  fi
  local pdf
  pdf=$(find "$PROJECT_ROOT/share/figure/$ANA_NAME" -name '*checkHist*.pdf' -mmin -10 2>/dev/null | head -1 || true)
  if [[ -z "$pdf" || ! -f "$pdf" ]]; then
    log_msg "WARNING: checkHist PDF not found"
    return 1
  fi
  cp -f "$pdf" "$dest_pdf"
  log_msg "Copied PDF to $dest_pdf"
  return 0
}

format_id_sample() {
  local -a ids=("$@")
  local n="${#ids[@]}"
  local max=40
  if (( n == 0 )); then
    echo "(none)"
    return
  fi
  local shown=("${ids[@]:0:$max}")
  local s
  s=$(IFS=,; echo "${shown[*]}")
  if (( n > max )); then
    echo "${s},... (+$((n - max)) more)"
  else
    echo "$s"
  fi
}

build_progress_text() {
  local phase="$1"
  local found="$2"
  local condor_line="$3"
  local newly="$4"
  cat <<EOF
**[mysubmit/$phase]** **${ANA_NAME}** \`jobid=${JOBID}\`
ROOT: **${found}/${EXPECTED}**
Condor: ${condor_line}
Newly left queue: ${newly}
Scratch: \`${SCRATCH_BASE}\`
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --runmeta) RUNMETA="$2"; shift 2 ;;
    --ana-name) ANA_NAME="$2"; shift 2 ;;
    --jobid) JOBID="$2"; shift 2 ;;
    --joblist) JOBLIST="$2"; shift 2 ;;
    --mainconf) MAINCONF="$2"; shift 2 ;;
    --poll-sec) POLL_SEC="$2"; shift 2 ;;
    --progress-sec) PROGRESS_SEC="$2"; shift 2 ;;
    --timeout-hours) TIMEOUT_SEC="$(( $2 * 3600 ))"; shift 2 ;;
    --allow-partial) ALLOW_PARTIAL=1; shift ;;
    --force-merge) FORCE_MERGE=1; shift ;;
    --exclude-bad-roots) EXCLUDE_BAD_ROOTS="$2"; shift 2 ;;
    -h|--help) usage; exit 0 ;;
    *) die "Unknown option: $1" ;;
  esac
done

[[ -n "$PYTHON" ]] || die "python3/python is required"
require_scratch

if [[ -n "$RUNMETA" ]]; then
  [[ -f "$RUNMETA" ]] || die "runmeta not found: $RUNMETA"
  PROJECT_ROOT="$(read_runmeta_field projectRoot)"
  ANA_NAME="$(read_runmeta_field anaName)"
  JOBID="$(read_runmeta_field jobid)"
  MAINCONF="$(read_runmeta_field mainconf)"
  JOBLIST="$(read_runmeta_field joblistSnapshot)"
  JOB_RUN_DIR="$PROJECT_ROOT/job/run"
fi

[[ -n "$ANA_NAME" && -n "$JOBID" ]] || { usage >&2; die "require --runmeta or both --ana-name and --jobid"; }
[[ -n "$JOBLIST" && -f "$JOBLIST" ]] || die "joblist snapshot not found: ${JOBLIST:-<unset>}"
[[ -n "$MAINCONF" ]] || die "mainconf not found in runmeta or --mainconf"

SCRATCH_BASE="$(scratch_base_for "$ANA_NAME" "$JOBID")"
WATCH_DIR="$SCRATCH_BASE/watch"
PROGRESS_DIR="$SCRATCH_BASE/progress"
mkdir -p "$WATCH_DIR" "$PROGRESS_DIR" "$SCRATCH_BASE/log" "$SCRATCH_BASE/err"

WATCH_LOG="$WATCH_DIR/watchmerge_mine.log"
PID_FILE="$WATCH_DIR/watchmerge_mine.pid"
DISCORD_MSG_FILE="$WATCH_DIR/discord_message_id"
CONDOR_SET_FILE="$WATCH_DIR/condor_procs.txt"
SIZE_CACHE_FILE="$WATCH_DIR/root_size_cache.txt"
PROGRESS_PDF="$PROGRESS_DIR/progress_checkHist.pdf"

if [[ -f "$PID_FILE" ]]; then
  old_pid="$(cat "$PID_FILE" 2>/dev/null || true)"
  if [[ -n "$old_pid" ]] && kill -0 "$old_pid" 2>/dev/null; then
    die "personal watch-merge already running (PID $old_pid, log: $WATCH_LOG)"
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
log_msg "personal watch-merge started anaName=$ANA_NAME jobid=$JOBID expectedSubjobs=$EXPECTED"
log_msg "pollSec=$POLL_SEC progressSec=$PROGRESS_SEC timeoutSec=$TIMEOUT_SEC"
log_msg "scratchBase=$SCRATCH_BASE rootfileDir=$ROOTFILE_DIR mergeOutput=$MERGE_OUTPUT"

EXTRA_JSON="$(SCRATCH_BASE="$SCRATCH_BASE" "$PYTHON" -c 'import json,os; print(json.dumps({"scratchBase": os.environ["SCRATCH_BASE"], "watcher": "mine"}))')"

discord_upsert "$(build_progress_text started 0 "querying..." "(init)")"

if [[ -n "$EXCLUDE_BAD_ROOTS" ]]; then
  [[ -f "$EXCLUDE_BAD_ROOTS" ]] || die "exclude list not found: $EXCLUDE_BAD_ROOTS"
fi

if [[ -f "$MERGE_OUTPUT" && "$FORCE_MERGE" -eq 0 ]]; then
  log_msg "merge output already exists; skipping merge: $MERGE_OUTPUT"
  update_runmeta_status "skipped_existing" "$EXPECTED" "$EXPECTED" "$MERGE_OUTPUT" "$WATCH_LOG" \
    "$STARTED_AT" "$(date -u +%Y-%m-%dT%H:%M:%SZ)" "$EXTRA_JSON"
  discord_upsert "$(build_progress_text skipped_existing "$EXPECTED" "n/a" "(existing merge)")"
  if run_checkhist_pdf "$MERGE_OUTPUT" "$PROGRESS_DIR/final_checkHist.pdf"; then
    discord_upsert "$(build_progress_text skipped_existing "$EXPECTED" "n/a" "(existing merge + PDF)")" \
      "$PROGRESS_DIR/final_checkHist.pdf"
  fi
  exit 0
fi

deadline=$(( $(date +%s) + TIMEOUT_SEC ))
last_progress_at=0
stable_count=-1
stable_polls=0
CONDOR_QUERY_OK=0
CURR_CONDOR_IDS=()
NEWLY_LEFT=()
PREV_CONDOR_IDS=()
load_prev_condor_set

while true; do
  now=$(date +%s)
  if (( now >= deadline )); then
    found="$(helper --rootfile-dir-from-joblist "$JOBLIST" --count-subjob-roots \
      --watch-merge-jobid "$JOBID")"
    log_msg "timeout after ${TIMEOUT_SEC}s (found $found/$EXPECTED)"
    discord_upsert "$(build_progress_text timeout "$found" "timeout" "n/a")"
    update_runmeta_status "timeout" "$EXPECTED" "$found" "" "$WATCH_LOG" \
      "$STARTED_AT" "$(date -u +%Y-%m-%dT%H:%M:%SZ)" "$EXTRA_JSON"
    exit 1
  fi

  found="$(helper --rootfile-dir-from-joblist "$JOBLIST" --count-subjob-roots \
    --watch-merge-jobid "$JOBID")"

  qraw="$(query_condor_procs)"
  CURR_CONDOR_IDS=()
  CONDOR_QUERY_OK=0
  condor_line="unavailable (ROOT-count fallback)"
  if [[ "$qraw" == OK* ]]; then
    CONDOR_QUERY_OK=1
    # shellcheck disable=SC2206
    CURR_CONDOR_IDS=(${qraw#OK })
    condor_line="remaining **${#CURR_CONDOR_IDS[@]}** / expected $EXPECTED"
  elif [[ "$qraw" == FAIL ]]; then
    CONDOR_QUERY_OK=0
  fi

  set_diff_left "${PREV_CONDOR_IDS[@]+"${PREV_CONDOR_IDS[@]}"}"
  newly_sample="$(format_id_sample "${NEWLY_LEFT[@]+"${NEWLY_LEFT[@]}"}")"
  delta_n="${#NEWLY_LEFT[@]}"

  log_msg "poll found=$found/$EXPECTED condorOk=$CONDOR_QUERY_OK condorRemain=${#CURR_CONDOR_IDS[@]} newlyLeft=$delta_n stablePolls=$stable_polls"

  if (( delta_n > 0 )); then
    discord_upsert "$(build_progress_text progress "$found" "$condor_line" "$newly_sample")"
  fi

  save_condor_set "${CURR_CONDOR_IDS[@]+"${CURR_CONDOR_IDS[@]}"}"
  PREV_CONDOR_IDS=("${CURR_CONDOR_IDS[@]+"${CURR_CONDOR_IDS[@]}"}")

  # Hourly progress hadd + PDF (non-destructive).
  if (( now - last_progress_at >= PROGRESS_SEC )); then
    last_progress_at=$now
    attach=""
    if run_progress_hadd; then
      if run_checkhist_pdf "$PROGRESS_DIR/partial_merge.root" "$PROGRESS_PDF"; then
        attach="$PROGRESS_PDF"
      fi
    fi
    discord_upsert "$(build_progress_text progress "$found" "$condor_line" "$newly_sample")" ${attach:+"$attach"}
  fi

  ready=0
  if (( CONDOR_QUERY_OK == 1 )); then
    if (( ${#CURR_CONDOR_IDS[@]} == 0 && found >= EXPECTED )); then
      if (( found == stable_count )); then
        stable_polls=$((stable_polls + 1))
      else
        stable_count=$found
        stable_polls=1
      fi
      if (( stable_polls >= 2 )); then
        ready=1
        log_msg "Condor empty for JOBID and ROOT count settled ($found/$EXPECTED)"
      fi
    elif (( ${#CURR_CONDOR_IDS[@]} == 0 && found < EXPECTED )); then
      log_msg "WARNING: Condor empty for JOBID but ROOT $found/$EXPECTED; waiting"
      discord_upsert "$(build_progress_text warn_roots_short "$found" "$condor_line" "$newly_sample")"
      stable_count=-1
      stable_polls=0
    else
      stable_count=-1
      stable_polls=0
    fi
  else
    # Fallback: official ROOT-count completion.
    if (( found >= EXPECTED )); then
      if (( found == stable_count )); then
        stable_polls=$((stable_polls + 1))
      else
        stable_count=$found
        stable_polls=1
      fi
      if (( stable_polls >= 2 )); then
        ready=1
      fi
    elif (( ALLOW_PARTIAL == 1 && found > 0 )); then
      if (( found == stable_count )); then
        stable_polls=$((stable_polls + 1))
      else
        stable_count=$found
        stable_polls=1
      fi
      if (( stable_polls >= 2 )); then
        ready=1
      fi
    else
      stable_count=-1
      stable_polls=0
    fi
  fi

  if (( ready == 1 )); then
    break
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
      "$STARTED_AT" "$(date -u +%Y-%m-%dT%H:%M:%SZ)" "$EXTRA_JSON"
    discord_upsert "$(build_progress_text failed_no_sample "$found" "$condor_line" "n/a")"
    exit 1
  fi
fi

log_msg "running final merge_root_files.csh on $MERGE_SAMPLE"
discord_upsert "$(build_progress_text merging "$found" "$condor_line" "starting final merge")"
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
    "$STARTED_AT" "$FINISHED_AT" "$EXTRA_JSON"
  discord_upsert "$(build_progress_text merge_failed "$found" "$condor_line" "rc=$merge_rc")"
  exit "$merge_rc"
fi

if [[ ! -f "$MERGE_OUTPUT" ]]; then
  log_msg "merge finished but output missing: $MERGE_OUTPUT"
  update_runmeta_status "failed_no_output" "$EXPECTED" "$found" "" "$WATCH_LOG" \
    "$STARTED_AT" "$FINISHED_AT" "$EXTRA_JSON"
  discord_upsert "$(build_progress_text failed_no_output "$found" "$condor_line" "missing merge root")"
  exit 1
fi

log_msg "merge completed: $MERGE_OUTPUT"
update_runmeta_status "ok" "$EXPECTED" "$found" "$MERGE_OUTPUT" "$WATCH_LOG" \
  "$STARTED_AT" "$FINISHED_AT" "$EXTRA_JSON"

final_attach=""
if run_checkhist_pdf "$MERGE_OUTPUT" "$PROGRESS_DIR/final_checkHist.pdf"; then
  final_attach="$PROGRESS_DIR/final_checkHist.pdf"
fi
discord_upsert "$(cat <<EOF
**[mysubmit/done]** **${ANA_NAME}** \`jobid=${JOBID}\`
Merge OK: \`${MERGE_OUTPUT}\`
ROOT: **${found}/${EXPECTED}**
EOF
)" ${final_attach:+"$final_attach"}

exit 0
