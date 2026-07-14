# Reproducibility checklist — `<anaName>`

Use when drafting Appendix A or before freezing a note. Gather from repo artifacts; **do not invent** values.

## Configuration

- [ ] `config/mainconf/main_<anaName>.yaml` path recorded
- [ ] Referenced YAMLs listed (cuts, hist, mixing, centrality, femto, etc.)
- [ ] Cut values match YAML (not hardcoded in Maker/macro per `PHILOSOPHY.md`)
- [ ] `analysis_info` / `anaName` consistent across configs and outputs

## Batch run provenance

- [ ] `jobid` (32-char hex)
- [ ] `job/run/configlog/config_<anaName>_<jobid>.txt`
- [ ] `job/run/joblistlog/joblist_<anaName>_<jobid>.xml`
- [ ] `job/run/runmeta/runmeta_<anaName>_<jobid>.json`
- [ ] Git commit / diff from runmeta sidecars (`gitstatus_*`, `gitdiff_*`)
- [ ] Submit stdout archived if needed for debugging

## Code / build

- [ ] `baseRunMacro` / `baseAnaMacro` names
- [ ] Maker library (`lib/libSt*Maker.so`) built with batch-matched toolchain (`sl7` + `make` or `./script/singularity_make.sh <mainconf>`)
- [ ] ACLiC / linked libs documented if non-default

## Data inputs

- [ ] Input file list or catalog reference
- [ ] Merge ROOT path: `rootfile/<anaName>/`
- [ ] Random seeds / mixing seed policy (if applicable)

## Outputs and QA

- [ ] QA PDF: `share/figure/<anaName>/`
- [ ] CheckHist / fit macro names and invocation
- [ ] Figure ↔ note cross-references in `analysisnote/<anaName>/assets/`

## Cross-links

- [ ] Daily log entry linked (`analysisnote/YYYYMMDD/summaryYYYYMMDD.md` via `daily-analysis-log`)
- [ ] Open questions tracker updated (`open-questions.md`)
