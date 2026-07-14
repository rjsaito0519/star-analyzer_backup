#!/bin/bash
# Capture submit-time reproducibility metadata for one jobid.
# Usage: ./snapshot_runmeta.sh <anaName> <jobid> <submitted_xml> <mainconf_rel> [submit_stdout_path]

set -euo pipefail

anaName="${1:?Usage: $0 <anaName> <jobid> <submitted_xml> <mainconf_rel> [submit_stdout_path]}"
jobid="${2:?Usage: $0 <anaName> <jobid> <submitted_xml> <mainconf_rel> [submit_stdout_path]}"
submitted_xml="${3:?Usage: $0 <anaName> <jobid> <submitted_xml> <mainconf_rel> [submit_stdout_path]}"
mainconf_rel="${4:?Usage: $0 <anaName> <jobid> <submitted_xml> <mainconf_rel> [submit_stdout_path]}"
submit_stdout="${5:-}"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
RUNMETA_DIR="$SCRIPT_DIR/runmeta"
PREFIX="${anaName}${jobid}"
PYTHON="$(command -v python3 2>/dev/null || command -v python 2>/dev/null)"

if [[ -z "$PYTHON" ]]; then
  echo "ERROR: python3/python is required for snapshot_runmeta.sh" >&2
  exit 1
fi

if [[ ! -f "$submitted_xml" ]]; then
  echo "ERROR: submitted XML not found: $submitted_xml" >&2
  exit 1
fi
submitted_xml="$(cd "$(dirname "$submitted_xml")" && pwd)/$(basename "$submitted_xml")"

mainconf_rel="${mainconf_rel#./}"
mainconf_path="$PROJECT_ROOT/$mainconf_rel"
if [[ ! -f "$mainconf_path" ]]; then
  echo "ERROR: mainconf not found: $mainconf_path" >&2
  exit 1
fi

mkdir -p "$RUNMETA_DIR"

runmeta_json="$RUNMETA_DIR/runmeta_${anaName}_${jobid}.json"
gitstatus_path="$RUNMETA_DIR/gitstatus_${anaName}_${jobid}.txt"
gitdiff_path="$RUNMETA_DIR/gitdiff_${anaName}_${jobid}.patch"
submit_stdout_snapshot="$RUNMETA_DIR/submit_stdout_${anaName}_${jobid}.txt"
runtime_bundle_path="$RUNMETA_DIR/runtime_bundle_${anaName}_${jobid}.tar.gz"
sums_artifacts_path="$RUNMETA_DIR/sums_artifacts_${anaName}_${jobid}.tar.gz"
config_snapshot="$SCRIPT_DIR/configlog/config_${anaName}_${jobid}.txt"
joblist_snapshot="$SCRIPT_DIR/joblistlog/joblist_${anaName}_${jobid}.xml"
report_path="$SCRIPT_DIR/${PREFIX}.report"
session_path="$SCRIPT_DIR/${PREFIX}.session.xml"

if [[ -n "$submit_stdout" && -f "$submit_stdout" ]]; then
  cp "$submit_stdout" "$submit_stdout_snapshot"
fi

(
  cd "$PROJECT_ROOT"
  git status --porcelain=v2 --branch > "$gitstatus_path" 2>/dev/null || true
  git diff --binary HEAD --submodule=diff > "$gitdiff_path" 2>/dev/null || true
)

bundle_entries=()
for entry in analysis config include StMaker StRoot lib src Makefile .gitmodules .sl73_x8664_gcc485; do
  if [[ -e "$PROJECT_ROOT/$entry" ]]; then
    bundle_entries+=("$entry")
  fi
done

if [[ ${#bundle_entries[@]} -gt 0 ]]; then
  (
    cd "$PROJECT_ROOT"
    tar --dereference --exclude='analysis/*_C.*' -czf "$runtime_bundle_path" "${bundle_entries[@]}"
  )
fi

# --files-from avoids "Argument list too long" when many runs create many PREFIX* files.
sums_list="$(mktemp)"
find "$SCRIPT_DIR" -maxdepth 1 -type f -name "${PREFIX}*" -printf '%f\n' | sort > "$sums_list"
if [[ -s "$sums_list" ]]; then
  tar -czf "$sums_artifacts_path" -C "$SCRIPT_DIR" --files-from="$sums_list"
fi
rm -f "$sums_list"

analysis_info_rel="$("$PYTHON" - "$mainconf_path" <<'PY'
import re
import sys

mainconf_path = sys.argv[1]
pattern = re.compile(r'^\s*analysis\s*:\s*([^\s#]+)')
with open(mainconf_path, 'r') as handle:
    for line in handle:
        match = pattern.search(line)
        if match:
            print(match.group(1))
            break
PY
)"

library_tag="$(cd "$PROJECT_ROOT" && "$PYTHON" script/analysis_info_helper.py --library-tag --mainconf "$mainconf_rel" 2>/dev/null | xargs || true)"

setup_probe="$(bash -lc "cd \"$PROJECT_ROOT\" && source ./script/setup.sh \"$mainconf_rel\" >/dev/null 2>&1; echo \"STAR_HOST_SYS=\${STAR_HOST_SYS:-}\"; echo \"ROOT4STAR=\$(command -v root4star 2>/dev/null || true)\"; echo \"ROOT_CONFIG=\$(command -v root-config 2>/dev/null || true)\"; echo \"ROOT_VERSION=\$(root-config --version 2>/dev/null || true)\"" 2>/dev/null || true)"
star_host_sys="$(printf "%s\n" "$setup_probe" | sed -n 's/^STAR_HOST_SYS=//p' | head -1)"
root4star_path="$(printf "%s\n" "$setup_probe" | sed -n 's/^ROOT4STAR=//p' | head -1)"
root_config_path="$(printf "%s\n" "$setup_probe" | sed -n 's/^ROOT_CONFIG=//p' | head -1)"
root_version="$(printf "%s\n" "$setup_probe" | sed -n 's/^ROOT_VERSION=//p' | head -1)"

git_commit="$(cd "$PROJECT_ROOT" && git rev-parse HEAD 2>/dev/null || true)"
git_short_commit="$(cd "$PROJECT_ROOT" && git rev-parse --short HEAD 2>/dev/null || true)"
git_branch="$(cd "$PROJECT_ROOT" && git symbolic-ref --short -q HEAD 2>/dev/null || true)"
git_dirty="false"
if [[ -s "$gitstatus_path" ]] && grep -q '^[12u?]' "$gitstatus_path"; then
  git_dirty="true"
fi

submodule_status_path="$RUNMETA_DIR/gitsubmodules_${anaName}_${jobid}.txt"
(cd "$PROJECT_ROOT" && git submodule status --recursive > "$submodule_status_path" 2>/dev/null || true)

created_at="$(date -u +%Y-%m-%dT%H:%M:%SZ)"
submit_host="$(hostname -f 2>/dev/null || hostname)"
submit_user="$(whoami)"

RUNMETA_JSON="$runmeta_json" \
PROJECT_ROOT="$PROJECT_ROOT" \
ANA_NAME="$anaName" \
JOB_ID="$jobid" \
JOB_PREFIX="$PREFIX" \
CREATED_AT="$created_at" \
SUBMIT_HOST="$submit_host" \
SUBMIT_USER="$submit_user" \
MAINCONF_REL="$mainconf_rel" \
ANALYSIS_INFO_REL="$analysis_info_rel" \
LIBRARY_TAG="$library_tag" \
STAR_HOST_SYS_VALUE="$star_host_sys" \
ROOT4STAR_PATH="$root4star_path" \
ROOT_CONFIG_PATH="$root_config_path" \
ROOT_VERSION_VALUE="$root_version" \
GIT_COMMIT="$git_commit" \
GIT_SHORT_COMMIT="$git_short_commit" \
GIT_BRANCH="$git_branch" \
GIT_DIRTY="$git_dirty" \
CONFIG_SNAPSHOT="$config_snapshot" \
JOBLIST_SNAPSHOT="$joblist_snapshot" \
GITSTATUS_PATH="$gitstatus_path" \
GITDIFF_PATH="$gitdiff_path" \
SUBMODULE_STATUS_PATH="$submodule_status_path" \
SUBMIT_STDOUT_SNAPSHOT="$submit_stdout_snapshot" \
RUNTIME_BUNDLE_PATH="$runtime_bundle_path" \
SUMS_ARTIFACTS_PATH="$sums_artifacts_path" \
REPORT_PATH="$report_path" \
SESSION_PATH="$session_path" \
SUBMITTED_XML="$submitted_xml" \
PYTHON_BIN="$PYTHON" \
"$PYTHON" - <<'PY'
import hashlib
import json
import os


def file_meta(path):
    if not path or not os.path.exists(path):
        return None
    sha256 = hashlib.sha256()
    with open(path, "rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            sha256.update(chunk)
    return {
        "path": path,
        "sizeBytes": os.path.getsize(path),
        "sha256": sha256.hexdigest(),
    }


def read_lines(path):
    if not path or not os.path.exists(path):
        return []
    with open(path, "r") as handle:
        return [line.rstrip("\n") for line in handle]


runmeta_path = os.environ["RUNMETA_JSON"]
manifest = {
    "schemaVersion": 1,
    "createdAt": os.environ["CREATED_AT"],
    "anaName": os.environ["ANA_NAME"],
    "jobid": os.environ["JOB_ID"],
    "jobPrefix": os.environ["JOB_PREFIX"],
    "projectRoot": os.environ["PROJECT_ROOT"],
    "submit": {
        "user": os.environ["SUBMIT_USER"],
        "host": os.environ["SUBMIT_HOST"],
        "submittedXml": file_meta(os.environ["SUBMITTED_XML"]),
        "configSnapshot": file_meta(os.environ["CONFIG_SNAPSHOT"]),
        "joblistSnapshot": file_meta(os.environ["JOBLIST_SNAPSHOT"]),
        "submitStdout": file_meta(os.environ["SUBMIT_STDOUT_SNAPSHOT"]),
        "reportPath": os.environ["REPORT_PATH"] if os.path.exists(os.environ["REPORT_PATH"]) else None,
        "sessionPath": os.environ["SESSION_PATH"] if os.path.exists(os.environ["SESSION_PATH"]) else None,
        "mainconf": os.environ["MAINCONF_REL"],
        "analysisInfo": os.environ["ANALYSIS_INFO_REL"] or None,
    },
    "runtime": {
        "libraryTag": os.environ["LIBRARY_TAG"] or None,
        "starHostSys": os.environ["STAR_HOST_SYS_VALUE"] or None,
        "root4starPath": os.environ["ROOT4STAR_PATH"] or None,
        "rootConfigPath": os.environ["ROOT_CONFIG_PATH"] or None,
        "rootVersion": os.environ["ROOT_VERSION_VALUE"] or None,
        "singularityImage": "/cvmfs/singularity.opensciencegrid.org/star-bnl/star-sw:latest",
        "runtimeBundle": file_meta(os.environ["RUNTIME_BUNDLE_PATH"]),
        "sumsArtifacts": file_meta(os.environ["SUMS_ARTIFACTS_PATH"]),
    },
    "git": {
        "commit": os.environ["GIT_COMMIT"] or None,
        "shortCommit": os.environ["GIT_SHORT_COMMIT"] or None,
        "branch": os.environ["GIT_BRANCH"] or None,
        "detachedHead": not bool(os.environ["GIT_BRANCH"]),
        "isDirty": os.environ["GIT_DIRTY"].lower() == "true",
        "status": file_meta(os.environ["GITSTATUS_PATH"]),
        "diffAgainstHead": file_meta(os.environ["GITDIFF_PATH"]),
        "submoduleStatusFile": file_meta(os.environ["SUBMODULE_STATUS_PATH"]),
        "submoduleStatus": read_lines(os.environ["SUBMODULE_STATUS_PATH"]),
    },
}

with open(runmeta_path, "w") as handle:
    json.dump(manifest, handle, indent=2, sort_keys=True)
    handle.write("\n")
PY

echo "Wrote runmeta snapshot: $runmeta_json"
