# AI rules/skills migration plan

## Agreed decisions

- Use split skill sources under `docs/ai/skills/<skill-name>.md`.
- Keep `PHILOSOPHY.md` in repository root.
- Consolidate rule source into `docs/ai/AGENT_RULES.md`.
- Keep `.cursor/rules/README-philosophy.mdc` as `alwaysApply: true` lightweight wrapper.

## Target architecture

- Source-of-truth:
  - `PHILOSOPHY.md`
  - `docs/ai/AGENT_RULES.md`
  - `docs/ai/skills/*.md`
- Antigravity entrypoint:
  - `CLAUDE.md` references the source-of-truth docs.
- Cursor integration:
  - `.cursor/rules/*.mdc` become wrappers.
  - `.cursor/skills/*/SKILL.md` keep metadata and reference `docs/ai/skills/*`.

## Execution order

1. Create source documents in `docs/ai`.
2. Move rule body content into `AGENT_RULES.md`.
3. Move skill body content into `docs/ai/skills/`.
4. Add `CLAUDE.md` at repo root with required reading order.
5. Convert `.cursor/rules` and `.cursor/skills` to wrappers.
6. Align `README.md` and `PHILOSOPHY.md` references and verify behavior parity.

## Synchronization policy

- Edit long-form guidance only in `docs/ai` and `PHILOSOPHY.md`.
- Keep `.cursor/*` wrappers concise (summary + pointer), not full copies.
- Optional future enforcement: link checks and wrapper metadata checks in pre-commit/CI.

## Key risks and mitigation

- Risk: wrapper files become too thin and reduce Cursor context quality.
- Mitigation: keep concise operational summaries in wrappers plus canonical links.
