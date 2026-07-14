# Skill: update-readme-scripts

Updates documentation when scripts or run workflow change.

## Primary targets

- `../../../docs/REFERENCE.md`: detailed script list and usage flow
- `../../../INSTALL.md`: first-time setup/run path when onboarding changes
- `../../../README.md`: high-level index/layout updates only
- `../../../job/run/README.md`: submit, configlog, cleanup, archive behavior

## Checklist

1. Reflect new/changed scripts in REFERENCE directory and run sections.
2. Ensure batch submit and configlog behavior are documented.
3. If onboarding changed, update INSTALL steps.
4. If documentation map changed, update README index rows.
5. Keep naming consistent (`anaName`, `jobid`, `configlog` path patterns).

## Principle

Favor clone-to-run reproducibility and avoid duplicating details across multiple docs.
