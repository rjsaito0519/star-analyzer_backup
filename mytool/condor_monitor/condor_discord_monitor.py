#!/usr/bin/env python3
import os
import sys
import re
import json
import time
import subprocess
import urllib.request
import urllib.parse
from datetime import datetime, timezone, timedelta

# Configuration
USER_NAME = "rjsaito"
POLL_INTERVAL_ACTIVE = 300  # 5 minutes
POLL_INTERVAL_IDLE = 900    # 15 minutes

# File paths relative to this script
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
WEBHOOK_FILE = os.path.join(SCRIPT_DIR, ".condor_discord_webhook")
STATE_FILE = os.path.join(SCRIPT_DIR, ".condor_state.json")

def secure_webhook_file():
    """Ensure the webhook file is only readable by the owner."""
    if os.path.exists(WEBHOOK_FILE):
        try:
            mode = os.stat(WEBHOOK_FILE).st_mode
            if mode & 0o077:  # Group or others have permissions
                print(f"Securing permissions of {WEBHOOK_FILE} to 600...")
                os.chmod(WEBHOOK_FILE, 0o600)
        except Exception as e:
            print(f"Warning: failed to secure permissions of webhook file: {e}")

def get_webhook_url():
    """Read the webhook URL from the file."""
    if not os.path.exists(WEBHOOK_FILE):
        print(f"Error: Webhook file {WEBHOOK_FILE} not found.", file=sys.stderr)
        print("Please write your Discord Webhook URL into it.", file=sys.stderr)
        sys.exit(1)
    
    secure_webhook_file()
    with open(WEBHOOK_FILE, "r") as f:
        url = f.read().strip()
    
    if not url.startswith("https://"):
        print(f"Error: Invalid Webhook URL in {WEBHOOK_FILE}", file=sys.stderr)
        sys.exit(1)
    return url

def run_condor_q():
    """Run condor_q for the user and return the stdout."""
    try:
        res = subprocess.run(
            ["condor_q", USER_NAME],
            capture_output=True,
            text=True,
            check=True
        )
        return res.stdout
    except Exception as e:
        print(f"Error running condor_q: {e}", file=sys.stderr)
        return None

def parse_condor_status(output):
    """Parse condor_q output and return (jobs_list, summary_dict)."""
    if not output:
        return None, None
        
    jobs = []
    # Regex to match individual job lines:
    # 478696.10  rjsaito         7/2  10:44   1+00:06:54 R  10  171.0 sh -c exec' '/g
    job_pattern = re.compile(
        r"^(\d+\.\d+)\s+(\S+)\s+(\d+/\d+\s+\d+:\d+)\s+(\S+)\s+([A-Z])\s+",
        re.MULTILINE
    )
    
    for match in job_pattern.finditer(output):
        jobs.append({
            "id": match.group(1),
            "owner": match.group(2),
            "submitted": match.group(3),
            "runtime": match.group(4),
            "status": match.group(5),
        })

    # Summary line regex:
    summary_pattern = re.compile(
        r"Total for query:\s*(\d+)\s*jobs?;\s*(\d+)\s*completed,\s*(\d+)\s*removed,\s*(\d+)\s*idle,\s*(\d+)\s*running,\s*(\d+)\s*held",
        re.IGNORECASE
    )
    
    summary = {
        "total": 0,
        "completed": 0,
        "removed": 0,
        "idle": 0,
        "running": 0,
        "held": 0
    }
    
    for line in output.splitlines():
        match = summary_pattern.search(line)
        if match:
            summary = {
                "total": int(match.group(1)),
                "completed": int(match.group(2)),
                "removed": int(match.group(3)),
                "idle": int(match.group(4)),
                "running": int(match.group(5)),
                "held": int(match.group(6))
            }
            break
    else:
        # Fallback if there are no jobs
        if "Total for query: 0" in output:
            summary = {"total": 0, "completed": 0, "removed": 0, "idle": 0, "running": 0, "held": 0}
        else:
            return None, None
            
    return jobs, summary

def load_state():
    """Load previously saved monitor state."""
    if os.path.exists(STATE_FILE):
        try:
            with open(STATE_FILE, "r") as f:
                return json.load(f)
        except Exception:
            pass
    return {"last_message_id": None, "last_counts": None}

def save_state(state):
    """Save monitor state."""
    try:
        with open(STATE_FILE, "w") as f:
            json.dump(state, f, indent=2)
    except Exception as e:
        print(f"Error saving state: {e}", file=sys.stderr)

def send_discord_request(webhook_url, payload, message_id=None):
    """Send POST (new message) or PATCH (edit message) to Discord Webhook."""
    headers = {
        "Content-Type": "application/json",
        "User-Agent": "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/115.0.0.0 Safari/537.36"
    }
    
    if message_id:
        # Edit existing message: PATCH /webhooks/<id>/<token>/messages/<message_id>
        parsed = urllib.parse.urlparse(webhook_url)
        new_path = parsed.path.rstrip("/") + f"/messages/{message_id}"
        url = urllib.parse.urlunparse((
            parsed.scheme,
            parsed.netloc,
            new_path,
            parsed.params,
            parsed.query,
            parsed.fragment
        ))
        req_method = "PATCH"
    else:
        # Create new message: POST /webhooks/<id>/<token>?wait=true
        parsed = urllib.parse.urlparse(webhook_url)
        query = parsed.query
        if query:
            query += "&wait=true"
        else:
            query = "wait=true"
        url = urllib.parse.urlunparse((
            parsed.scheme,
            parsed.netloc,
            parsed.path,
            parsed.params,
            query,
            parsed.fragment
        ))
        req_method = "POST"

    data = json.dumps(payload).encode("utf-8")
    req = urllib.request.Request(url, data=data, headers=headers, method=req_method)
    
    try:
        with urllib.request.urlopen(req) as res:
            res_data = res.read().decode("utf-8")
            if res_data:
                return json.loads(res_data)
            return {}
    except Exception as e:
        print(f"Failed to send Discord notification ({req_method}): {e}", file=sys.stderr)
        return None

def status_color(summary):
    if summary["held"] > 0:
        return 0xE74C3C  # Red
    if summary["idle"] > 0:
        return 0xF1C40F  # Yellow
    if summary["running"] > 0:
        return 0x2ECC71  # Green
    return 0x95A5A6      # Gray

def build_status_card(jobs, summary, jst_str, nyt_str, finished=False):
    """Build the Discord Embed payload."""
    if finished:
        return {
            "content": f"🎉 **【完了】HTCondor 全ジョブが完了しました！** (終了: `{jst_str}` JST)",
            "embeds": [
                {
                    "title": f"🎉 HTCondor Dashboard — {USER_NAME}",
                    "description": "すべてのジョブが完了しました！",
                    "color": 0x3498DB,  # Blue
                    "fields": [
                        {
                            "name": "🕒 終了時刻",
                            "value": f"JST: `{jst_str}`\nNYT: `{nyt_str}`",
                            "inline": False
                        }
                    ],
                    "footer": {
                        "text": "condor_q monitor"
                    }
                }
            ]
        }
        
    job_lines = [
        f"`{j['id']:10}` `{j['status']}` `{j['runtime']:>10}`"
        for j in jobs[:10]
    ]
    
    longest = max(jobs, key=lambda j: j["runtime"], default=None)
    
    fields = [
        {
            "name": "📊 Summary",
            "value": (
                f"🟢 Running: **{summary['running']}**\n"
                f"🟡 Idle: **{summary['idle']}**\n"
                f"🔴 Held: **{summary['held']}**\n"
                f"⚪ Total Active: **{summary['total']}**"
            ),
            "inline": True,
        },
        {
            "name": "🕒 Updated",
            "value": (
                f"JST: `{jst_str}`\n"
                f"NYT: `{nyt_str}`"
            ),
            "inline": True,
        },
    ]
    
    if longest:
        fields.append({
            "name": "⏱ Longest Runtime",
            "value": f"`{longest['id']}`  `{longest['runtime']}`",
            "inline": False,
        })
        
    if job_lines:
        fields.append({
            "name": f"📋 Jobs (Showing {min(len(jobs), 10)}/{len(jobs)})",
            "value": "\n".join(job_lines),
            "inline": False,
        })
        
    if summary["held"] > 0:
        status_text = "⚠️ **警告: 保留(Held)ジョブあり**"
    elif summary["running"] > 0:
        status_text = "🔄 **ジョブ実行中...**"
    else:
        status_text = "💤 **アイドル状態**"
    content = f"{status_text} (更新: `{jst_str}` JST)"

    return {
        "content": content,
        "embeds": [
            {
                "title": f"HTCondor Dashboard — {USER_NAME}",
                "description": "このメッセージは自動更新されます。",
                "color": status_color(summary),
                "fields": fields,
                "footer": {
                    "text": "condor_q monitor"
                }
            }
        ]
    }

def main():
    webhook_url = get_webhook_url()
    state = load_state()
    
    print(f"Starting HTCondor monitor for user: {USER_NAME}")
    
    while True:
        stdout = run_condor_q()
        jobs, counts = parse_condor_status(stdout)
        
        if counts is None:
            # If condor_q fails or returns invalid output, sleep and try again
            time.sleep(60)
            continue
            
        # Get formatted times
        utc_now = datetime.now(timezone.utc)
        jst_now = utc_now + timedelta(hours=9)
        nyt_now = datetime.now()  # System time is Eastern Time (NYT)
        
        jst_str = jst_now.strftime("%Y-%m-%d %H:%M:%S")
        nyt_str = nyt_now.strftime("%Y-%m-%d %H:%M:%S")
        
        total_jobs = counts["total"]
        last_counts = state.get("last_counts")
        
        # Determine check interval based on active jobs
        interval = POLL_INTERVAL_ACTIVE if total_jobs > 0 else POLL_INTERVAL_IDLE
        
        if total_jobs == 0:
            # Case 1: No jobs active. If they just finished (last_counts > 0), notify!
            if last_counts and last_counts.get("total", 0) > 0:
                print("All jobs completed! Sending completion notification.")
                payload = build_status_card(jobs, counts, jst_str, nyt_str, finished=True)
                send_discord_request(webhook_url, payload)
                # Reset dashboard message ID so a new one is created next run
                state["last_message_id"] = None
                
            state["last_counts"] = counts
            save_state(state)
        else:
            # Case 2: Jobs are running. Update/create the dashboard message.
            payload = build_status_card(jobs, counts, jst_str, nyt_str)
            msg_id = state.get("last_message_id")
            
            if msg_id:
                print(f"Updating status message: {msg_id}")
                # Edit existing message
                res = send_discord_request(webhook_url, payload, message_id=msg_id)
                # If message was deleted or PATCH failed, recreate it
                if res is None:
                    print("Failed to update message, creating a new one.")
                    res = send_discord_request(webhook_url, payload)
                    if res and "id" in res:
                        state["last_message_id"] = res["id"]
            else:
                print("Creating new status message.")
                res = send_discord_request(webhook_url, payload)
                if res and "id" in res:
                    state["last_message_id"] = res["id"]
            
            state["last_counts"] = counts
            save_state(state)
        
        print(f"[{nyt_str}] Counts: {counts}. Sleeping for {interval}s...")
        time.sleep(interval)

if __name__ == "__main__":
    main()
