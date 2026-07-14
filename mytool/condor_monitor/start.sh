#!/bin/bash
# Start script for condor_discord_monitor.py

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PID_FILE="$SCRIPT_DIR/.monitor.pid"
MONITOR_PY="$SCRIPT_DIR/condor_discord_monitor.py"
LOG_FILE="$SCRIPT_DIR/monitor.log"

# Check if already running
if [ -f "$PID_FILE" ]; then
    PID=$(cat "$PID_FILE")
    if kill -0 "$PID" 2>/dev/null; then
        echo "Condor monitor is already running with PID $PID."
        exit 0
    else
        echo "Stale PID file found. Cleaning it up."
        rm -f "$PID_FILE"
    fi
fi

# Make python script executable
chmod +x "$MONITOR_PY"

# Check if webhook file exists
WEBHOOK_FILE="$SCRIPT_DIR/.condor_discord_webhook"
if [ ! -f "$WEBHOOK_FILE" ]; then
    echo "Warning: $WEBHOOK_FILE does not exist."
    echo "Please create this file and paste your Discord Webhook URL into it."
    echo "Example: echo 'https://discord.com/api/webhooks/...' > $WEBHOOK_FILE"
    echo "Then run this script again."
    # Create the placeholder and set chmod 600
    touch "$WEBHOOK_FILE"
    chmod 600 "$WEBHOOK_FILE"
    exit 1
fi

# Ensure webhook file permissions are 600
chmod 600 "$WEBHOOK_FILE"

# Run in background
nohup python3 -u "$MONITOR_PY" > "$LOG_FILE" 2>&1 &
NEW_PID=$!
echo $NEW_PID > "$PID_FILE"

echo "Condor monitor started in the background (PID: $NEW_PID)."
echo "Logs are written to: $LOG_FILE"
