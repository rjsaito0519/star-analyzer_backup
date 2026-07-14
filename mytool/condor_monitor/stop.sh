#!/bin/bash
# Stop script for condor_discord_monitor.py

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PID_FILE="$SCRIPT_DIR/.monitor.pid"

if [ ! -f "$PID_FILE" ]; then
    echo "PID file not found. Monitor might not be running."
    # Fallback check
    PIDS=$(pgrep -f "condor_discord_monitor.py" || true)
    if [ -n "$PIDS" ]; then
        echo "Found running processes matching 'condor_discord_monitor.py': $PIDS"
        echo "Killing them..."
        kill $PIDS
    else
        echo "No running monitor processes found."
    fi
    exit 0
fi

PID=$(cat "$PID_FILE")

if kill -0 "$PID" 2>/dev/null; then
    echo "Stopping Condor monitor (PID: $PID)..."
    kill "$PID"
    
    # Wait for process to exit
    for i in {1..5}; do
        if ! kill -0 "$PID" 2>/dev/null; then
            break
        fi
        sleep 1
    done
    
    if kill -0 "$PID" 2>/dev/null; then
        echo "Process did not stop. Forcing shutdown..."
        kill -9 "$PID"
    fi
    
    echo "Stopped."
else
    echo "Process with PID $PID was not running."
fi

rm -f "$PID_FILE"
