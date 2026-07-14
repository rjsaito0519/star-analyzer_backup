# STAR analysis agent entrypoint

Use this file as the Antigravity / Claude Code entrypoint for project guidance.

## Required reading order

1. `PHILOSOPHY.md`
2. `docs/ai/AGENT_RULES.md`
3. `docs/ai/skills/` (choose the relevant procedure file per task)

## Intent

- Keep StChain/StMaker architecture and YAML-driven reproducibility constraints intact.
- Use `mainconf` as the operational entry point for setup, run, and job tooling.
- Treat `docs/ai/*` and `PHILOSOPHY.md` as source-of-truth; `.cursor/*` files are wrappers for Cursor integration.

## Skill sync completion rule

- Source-of-truth for skills is `docs/ai/skills/*.md`.
- After adding or editing any file under `docs/ai/skills/`, always run:
  - `script/sync_and_check_skills.sh`
- Consider the task incomplete until the command passes.

## Human-facing references

- `README.md`
- `INSTALL.md`
- `docs/REFERENCE.md`
- `job/run/README.md`
