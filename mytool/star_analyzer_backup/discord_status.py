#!/usr/bin/env python3
"""Update two fixed Discord status messages via incoming webhook."""

from __future__ import annotations

import argparse
import json
import sys
import urllib.error
import urllib.request
from pathlib import Path


def webhook_base(url: str) -> str:
    return url.rstrip("/")


def load_message_id(path: Path) -> str | None:
    if not path.is_file():
        return None
    value = path.read_text(encoding="utf-8").strip()
    return value or None


def save_message_id(path: Path, message_id: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(message_id, encoding="utf-8")


def request_json(method: str, url: str, payload: dict) -> dict:
    data = json.dumps(payload).encode("utf-8")
    req = urllib.request.Request(
        url,
        data=data,
        headers={
            "Content-Type": "application/json",
            "User-Agent": "star-analyzer-backup/1.0",
        },
        method=method,
    )
    with urllib.request.urlopen(req, timeout=30) as resp:
        body = resp.read().decode("utf-8")
        return json.loads(body) if body else {}


def post_message(webhook_url: str, embed: dict) -> dict:
    url = f"{webhook_base(webhook_url)}?wait=true"
    return request_json("POST", url, {"embeds": [embed]})


def patch_message(webhook_url: str, message_id: str, embed: dict) -> dict:
    url = f"{webhook_base(webhook_url)}/messages/{message_id}"
    return request_json("PATCH", url, {"embeds": [embed]})


def update_message(
    webhook_url: str,
    message_id_file: Path,
    embed: dict,
) -> None:
    message_id = load_message_id(message_id_file)
    if message_id:
        try:
            patch_message(webhook_url, message_id, embed)
            return
        except urllib.error.HTTPError as exc:
            if exc.code != 404:
                raise

    message = post_message(webhook_url, embed)
    new_id = message.get("id")
    if not new_id:
        raise RuntimeError("Discord webhook did not return a message id")
    save_message_id(message_id_file, str(new_id))


def field(name: str, value: str, inline: bool = False) -> dict:
    text = value if value else "-"
    if len(text) > 1024:
        text = text[:1021] + "..."
    return {"name": name, "value": text, "inline": inline}


def build_sync_embed(args: argparse.Namespace) -> dict:
    if args.sync_ok == "true":
        color = 0x57F287 if args.files_changed != "0" else 0x5865F2
        title = "star-analyzer backup sync"
    else:
        color = 0xED4245
        title = "star-analyzer backup sync — error"

    return {
        "title": title,
        "color": color,
        "fields": [
            field("Status", args.sync_status),
            field("Last check", args.last_check, inline=True),
            field("Source branch", args.source_branch, inline=True),
            field("Files changed", args.files_changed, inline=True),
            field("Source", args.source),
            field("Destination", args.destination),
            field("Log", args.log_file),
        ],
        "footer": {"text": "Sync heartbeat (message is edited, not reposted)"},
    }


def build_commit_embed(args: argparse.Namespace) -> dict:
    if args.push_ok == "false":
        color = 0xED4245
        title = "star-analyzer backup git — push error"
    elif args.committed == "true":
        color = 0x57F287
        title = "star-analyzer backup git — committed"
    else:
        color = 0x5865F2
        title = "star-analyzer backup git — no changes"

    fields = [
        field("This run", args.run_summary),
        field("Last check", args.last_check, inline=True),
        field("Branch", args.github_branch, inline=True),
        field("Remote", args.github_remote),
    ]

    if args.committed == "true":
        fields.append(field("New commit", f"`{args.commit_hash}`\n{args.commit_summary}"))
    elif args.last_commit_hash:
        fields.append(
            field(
                "Last commit",
                f"`{args.last_commit_hash}`\n{args.last_commit_summary}",
            )
        )

    fields.append(field("Push", args.push_status))
    fields.append(field("Log", args.log_file))

    return {
        "title": title,
        "color": color,
        "fields": fields,
        "footer": {"text": "Git status (message is edited, not reposted)"},
    }


def main() -> int:
    parser = argparse.ArgumentParser(description="Update Discord backup status messages")
    sub = parser.add_subparsers(dest="command", required=True)

    sync = sub.add_parser("sync")
    sync.add_argument("--webhook-url", required=True)
    sync.add_argument("--message-id-file", required=True, type=Path)
    sync.add_argument("--sync-ok", choices=("true", "false"), required=True)
    sync.add_argument("--sync-status", required=True)
    sync.add_argument("--last-check", required=True)
    sync.add_argument("--source-branch", required=True)
    sync.add_argument("--files-changed", required=True)
    sync.add_argument("--source", required=True)
    sync.add_argument("--destination", required=True)
    sync.add_argument("--log-file", required=True)

    commit = sub.add_parser("commit")
    commit.add_argument("--webhook-url", required=True)
    commit.add_argument("--message-id-file", required=True, type=Path)
    commit.add_argument("--committed", choices=("true", "false"), required=True)
    commit.add_argument("--run-summary", required=True)
    commit.add_argument("--last-check", required=True)
    commit.add_argument("--github-branch", required=True)
    commit.add_argument("--github-remote", required=True)
    commit.add_argument("--commit-hash", default="")
    commit.add_argument("--commit-summary", default="")
    commit.add_argument("--last-commit-hash", default="")
    commit.add_argument("--last-commit-summary", default="")
    commit.add_argument("--push-status", required=True)
    commit.add_argument("--push-ok", choices=("true", "false"), required=True)
    commit.add_argument("--log-file", required=True)

    args = parser.parse_args()

    try:
        if args.command == "sync":
            embed = build_sync_embed(args)
            update_message(args.webhook_url, args.message_id_file, embed)
        else:
            embed = build_commit_embed(args)
            update_message(args.webhook_url, args.message_id_file, embed)
    except Exception as exc:  # noqa: BLE001
        print(f"discord update failed: {exc}", file=sys.stderr)
        return 1

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
