#!/usr/bin/env python3
"""Synchronize Cursor skill wrappers from docs/ai/skills sources.

This script treats docs/ai/skills/*.md as source-of-truth and writes
.cursor/skills/<skill-name>/SKILL.md wrappers.
"""

from __future__ import annotations

import argparse
import re
from pathlib import Path
from typing import Optional


FRONTMATTER_PATTERN = re.compile(r"^---\n(.*?)\n---\n", re.DOTALL)
DESCRIPTION_PATTERN = re.compile(r"^description:\s*(.+?)\s*$", re.MULTILINE)


def find_repo_root() -> Path:
    return Path(__file__).resolve().parent.parent


def parse_title_and_summary(source_text: str, fallback_name: str) -> tuple[str, str]:
    lines = source_text.splitlines()
    title = fallback_name
    for line in lines:
        if line.startswith("# "):
            title = line[2:].strip()
            break

    summary = ""
    saw_title = False
    for line in lines:
        if line.startswith("# "):
            saw_title = True
            continue
        if not saw_title:
            continue
        stripped = line.strip()
        if not stripped or stripped.startswith("#"):
            continue
        summary = stripped
        break
    return title, summary


def extract_existing_description(wrapper_path: Path) -> Optional[str]:
    if not wrapper_path.exists():
        return None
    text = wrapper_path.read_text(encoding="utf-8")
    frontmatter_match = FRONTMATTER_PATTERN.search(text)
    if not frontmatter_match:
        return None
    description_match = DESCRIPTION_PATTERN.search(frontmatter_match.group(1))
    if not description_match:
        return None
    return description_match.group(1).strip()


def build_default_description(skill_name: str, title: str, summary: str) -> str:
    if summary:
        return (
            f"{summary} Use when the user asks to run the '{title}' "
            f"workflow or related tasks."
        )
    return (
        "Runs the canonical procedure documented under docs/ai/skills. "
        f"Use when the user asks for '{title}' tasks."
    )


def build_wrapper_text(skill_name: str, source_filename: str, description: str) -> str:
    return (
        "---\n"
        f"name: {skill_name}\n"
        f"description: {description}\n"
        "---\n\n"
        f"# Skill wrapper: {skill_name}\n\n"
        "Canonical procedure:\n\n"
        f"- [docs/ai/skills/{source_filename}]"
        f"(../../../docs/ai/skills/{source_filename})\n\n"
        "Related source-of-truth:\n\n"
        "- [docs/ai/AGENT_RULES.md](../../../docs/ai/AGENT_RULES.md)\n"
        "- [PHILOSOPHY.md](../../../PHILOSOPHY.md)\n"
    )


def sync_skill(
    source_path: Path,
    cursor_skills_dir: Path,
    dry_run: bool,
    preserve_description: bool,
) -> bool:
    skill_name = source_path.stem
    wrapper_dir = cursor_skills_dir / skill_name
    wrapper_path = wrapper_dir / "SKILL.md"

    source_text = source_path.read_text(encoding="utf-8")
    title, summary = parse_title_and_summary(source_text, fallback_name=skill_name)
    description = build_default_description(skill_name, title, summary)

    if preserve_description:
        existing_description = extract_existing_description(wrapper_path)
        if existing_description:
            description = existing_description

    wrapper_text = build_wrapper_text(skill_name, source_path.name, description)
    current_text = wrapper_path.read_text(encoding="utf-8") if wrapper_path.exists() else None
    changed = current_text != wrapper_text

    if changed and not dry_run:
        wrapper_dir.mkdir(parents=True, exist_ok=True)
        wrapper_path.write_text(wrapper_text, encoding="utf-8")

    action = "update" if wrapper_path.exists() else "create"
    if changed:
        print(f"{action}: {wrapper_path}")
    else:
        print(f"ok: {wrapper_path}")
    return changed


def prune_stale_wrappers(
    docs_skills_dir: Path,
    cursor_skills_dir: Path,
    dry_run: bool,
) -> int:
    docs_names = {path.stem for path in docs_skills_dir.glob("*.md")}
    removed = 0
    if not cursor_skills_dir.exists():
        return removed

    for child in sorted(cursor_skills_dir.iterdir()):
        if not child.is_dir():
            continue
        wrapper_file = child / "SKILL.md"
        if child.name in docs_names or not wrapper_file.exists():
            continue
        removed += 1
        if dry_run:
            print(f"prune: {child}")
            continue
        wrapper_file.unlink()
        child.rmdir()
        print(f"prune: {child}")
    return removed


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Sync .cursor skill wrappers from docs/ai/skills sources."
    )
    parser.add_argument("--dry-run", action="store_true", help="Show changes without writing files.")
    parser.add_argument(
        "--refresh-description",
        action="store_true",
        help="Regenerate wrapper descriptions from docs (default preserves existing descriptions).",
    )
    parser.add_argument(
        "--prune",
        action="store_true",
        help="Remove wrapper directories that do not have matching docs/ai skill sources.",
    )
    args = parser.parse_args()

    repo_root = find_repo_root()
    docs_skills_dir = repo_root / "docs" / "ai" / "skills"
    cursor_skills_dir = repo_root / ".cursor" / "skills"

    if not docs_skills_dir.exists():
        print(f"error: missing docs skill directory: {docs_skills_dir}")
        return 1

    changed_count = 0
    sources = sorted(docs_skills_dir.glob("*.md"))
    for source_path in sources:
        changed = sync_skill(
            source_path=source_path,
            cursor_skills_dir=cursor_skills_dir,
            dry_run=args.dry_run,
            preserve_description=not args.refresh_description,
        )
        changed_count += int(changed)

    removed_count = 0
    if args.prune:
        removed_count = prune_stale_wrappers(
            docs_skills_dir=docs_skills_dir,
            cursor_skills_dir=cursor_skills_dir,
            dry_run=args.dry_run,
        )

    print(
        "done: "
        f"sources={len(sources)} changed={changed_count} "
        f"pruned={removed_count} dry_run={args.dry_run}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
