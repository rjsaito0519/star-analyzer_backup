#!/usr/bin/env bash
set -euo pipefail

python3 script/sync_cursor_skills.py
python3 script/check_cursor_skill_sync.py
