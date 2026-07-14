#!/bin/bash
# Create symlinks in job/run to analysis, config, lib, include, StMaker so that
# SUMS wrapper (which runs with cwd=job/run) can find them for the sandbox.
# Run from job/run. Idempotent: only creates links when missing.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

for d in analysis config lib include StMaker StRoot; do
  if [[ ! -e "$SCRIPT_DIR/$d" ]]; then
    ln -sf "../../$d" "$SCRIPT_DIR/$d"
  fi
done
