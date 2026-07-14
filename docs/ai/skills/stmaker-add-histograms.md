# Skill: stmaker-add-histograms

Adds histograms to `StPhiMaker` or another Maker using YAML-driven definitions.

## Steps

1. Update histogram YAML (`config/hist/*.yaml`):
   - Add axis entries if needed
   - Add histogram definitions and titles
2. Update Maker implementation:
   - Add `m_histManager->Fill("HistName", ...)` in appropriate event/pair scope
3. Update check macro (`common/macro/checkHistAnaPhi.C` or equivalent):
   - Add retrieval and draw panels for new histograms

## Conventions

- Histogram key names must match across YAML, `Fill(...)`, and `Get(...)`.
- Rebuild Maker when C++ changes; macro-only changes do not require library rebuild.
- On hosts where host `make` is unreliable, rebuild with **`./script/singularity_make.sh <mainconf>`** (SL7-equivalent / batch-matched toolchain inside `star-bnl/star-sw:latest`) per `singularity-local-build-run.md` instead of host `make`.
