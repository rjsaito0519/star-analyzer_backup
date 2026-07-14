#!/bin/bash
# Status check script for condor_discord_monitor.py

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PID_FILE="$SCRIPT_DIR/.monitor.pid"
LOG_FILE="$SCRIPT_DIR/monitor.log"

if [ -f "$PID_FILE" ]; then
    PID=$(cat "$PID_FILE")
    if kill -0 "$PID" 2>/dev/null; then
        echo -e "\e[32m● Condor monitor is RUNNING (PID: $PID)\e[0m"
        echo "----------------------------------------"
        echo "Last log entries ($LOG_FILE):"
        tail -n 5 "$LOG_FILE"
        exit 0
    else
        echo -e "\e[31m○ Condor monitor is NOT running (Stale PID file found)\e[0m"
        exit 1
    fi
else
    # Check if there is any stray process
    PIDS=$(pgrep -f "condor_discord_monitor.py" || true)
    if [ -n "$PIDS" ]; then
        echo -e "\e[33m● Condor monitor is RUNNING without PID file (PIDs: $PIDS)\e[0m"
        exit 0
    fi
    echo -e "\e[90m○ Condor monitor is STOPPED\e[0m"
    exit 0
fi
