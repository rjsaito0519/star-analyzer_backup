#!/usr/bin/env python3
"""Validate parity between docs/ai skills and .cursor skill wrappers."""

from __future__ import annotations

import re
from pathlib import Path


NAME_PATTERN = re.compile(r"^name:\s*([a-z0-9-]+)\s*$", re.MULTILINE)
LINK_PATTERN = re.compile(
    r"\[docs/ai/skills/([^\]]+)\]\(../../../docs/ai/skills/\1\)",
    re.MULTILINE,
)


def find_repo_root() -> Path:
    return Path(__file__).resolve().parent.parent


def read_text(path: Path) -> str:
    return path.read_text(encoding="utf-8")


def main() -> int:
    repo_root = find_repo_root()
    docs_dir = repo_root / "docs" / "ai" / "skills"
    cursor_dir = repo_root / ".cursor" / "skills"

    errors: list[str] = []

    if not docs_dir.exists():
        print(f"error: missing directory: {docs_dir}")
        return 1
    if not cursor_dir.exists():
        print(f"error: missing directory: {cursor_dir}")
        return 1

    docs_sources = sorted(docs_dir.glob("*.md"))
    docs_names = {p.stem for p in docs_sources}

    for source in docs_sources:
        skill_name = source.stem
        wrapper = cursor_dir / skill_name / "SKILL.md"
        if not wrapper.exists():
            errors.append(f"missing wrapper: {wrapper}")
            continue

        text = read_text(wrapper)

        name_match = NAME_PATTERN.search(text)
        if not name_match:
            errors.append(f"missing frontmatter name in: {wrapper}")
        elif name_match.group(1) != skill_name:
            errors.append(
                f"name mismatch in {wrapper}: "
                f"expected '{skill_name}', got '{name_match.group(1)}'"
            )

        link_match = LINK_PATTERN.search(text)
        expected_file = source.name
        if not link_match:
            errors.append(f"missing canonical docs link in: {wrapper}")
        elif link_match.group(1) != expected_file:
            errors.append(
                f"link mismatch in {wrapper}: expected docs file '{expected_file}'"
            )

    for child in sorted(cursor_dir.iterdir()):
        if not child.is_dir():
            continue
        wrapper = child / "SKILL.md"
        if not wrapper.exists():
            continue
        if child.name not in docs_names:
            errors.append(f"stale wrapper without docs source: {child}")

    if errors:
        print("cursor skill sync check failed:")
        for err in errors:
            print(f"- {err}")
        return 1

    print(
        "cursor skill sync check passed: "
        f"sources={len(docs_sources)} wrappers={len(docs_names)}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
