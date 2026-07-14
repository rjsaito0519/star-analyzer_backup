#!/bin/bash
# Generate joblist XML from mainconf and analysis info.
# Usage: ./script/generate_joblist.sh MAINCONF_PATH
# Example: ./script/generate_joblist.sh config/mainconf/main_auau19_anaLambda.yaml

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

if [ -z "${1:-}" ]; then
    echo "Usage: $0 MAINCONF_PATH" >&2
    echo "  MAINCONF_PATH  e.g. config/mainconf/main_auau19_anaLambda.yaml" >&2
    echo "  Output: job/joblist/joblist_<anaName>.xml (e.g. joblist_auau19_anaLambda.xml)" >&2
    exit 1
fi

# Prefer python3 (PyYAML installs cleanly; python2.7 may pull in wrong setuptools on some envs)
PYTHON=$(command -v python3 2>/dev/null || command -v python 2>/dev/null)
cd "$PROJECT_ROOT" && "$PYTHON" script/analysis_info_helper.py --generate-joblist "$1" || exit 1
