# Critical review checklist (generic)

Apply at Phase 2 (completeness audit) and at each section gate during drafting.

## Strictness levels

| Level | When to use | Gate behavior |
|-------|-------------|---------------|
| **light** | User requests quick pass | Flag only obvious gaps; fewer follow-up questions |
| **standard** (default) | Internal pre-review | Stop on mandatory items; question strongly recommended items |
| **rigorous** | User requests paper-review depth | Stop on all gaps including cross-checks and literature comparison |

## Mandatory stop (standard and rigorous)

- [ ] Observable and physics goal stated
- [ ] Collision system, energy, dataset identified
- [ ] Event selection cuts listed with values **and** justification
- [ ] Track selection cuts listed with values **and** justification
- [ ] PID criteria defined (dE/dx, TOF, combined) with efficiency impact noted
- [ ] Required corrections identified (acceptance, efficiency, resolution) — or explicit justification for omission
- [ ] Systematic uncertainty sources enumerated (not only statistical errors)
- [ ] Conclusions supported by presented data and quoted uncertainties
- [ ] No contradiction with stated conservation laws / symmetries without explanation

## Strongly recommended (standard: question + `[要確認]`; rigorous: stop)

- [ ] Cross-checks documented (cut variation, closure, independent method, alternate dataset)
- [ ] Comparison to prior measurements or theory (even qualitative)
- [ ] Statistical vs systematic error treatment clearly separated in tables and text
- [ ] Fit ranges and background model sensitivity addressed
- [ ] Low-statistics regions called out; no over-interpretation

## Suggestion only (all levels)

- [ ] Figure style, caption clarity, reference formatting
- [ ] Optional sensitivity studies beyond publication minimum
- [ ] Additional centrality / pT bins for completeness

## Probing questions (use when a box is unchecked)

- Why was this cut chosen? What happens at ±10–20% variation?
- Is this systematic source quantified or only mentioned?
- What would falsify this conclusion?
- What cross-check would convince a skeptical collaborator?
- Does the mixing / background / efficiency correction close on MC or embedding?

## When user says "intentional omission"

1. Ask for physical or operational justification.
2. If reasonable, record in **Intentional omissions** (`open-questions.md`) and proceed.
3. Do not re-flag the same item unless new evidence contradicts the justification.

## Warn mode

Only if user explicitly says **先に下書き** / **draft first**:

- Do not stop drafting; mark gaps `[要確認]` and register in Open Questions / To-Do.
- Phase 1 hearing is **still required** — do not skip proactive questions.
