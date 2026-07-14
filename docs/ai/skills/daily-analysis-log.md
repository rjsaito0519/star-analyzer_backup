# Skill: daily-analysis-log

Creates or extends a reproducibility note under `analysisnote/YYYYMMDD/summaryYYYYMMDD.md`.

## Workflow

1. Resolve date (`YYYYMMDD`) from user input or current date.
2. Set paths:
   - directory: `analysisnote/YYYYMMDD/`
   - file: `analysisnote/YYYYMMDD/summaryYYYYMMDD.md`
3. **Check whether the day's note already exists** (`summaryYYYYMMDD.md`).
   - If **missing**: create the directory (if needed) and write a new summary file.
   - If **present**: **append** a new session block to the existing file. Do **not** overwrite or replace prior content unless the user explicitly asks to rewrite the whole day.
4. Gather development summary from git history for the day.
5. Gather run evidence from `job/run/configlog/` when present.
6. Write content with date, development, studies, analyses run, and notes.

## Append format (when file already exists)

Add a visible separator and a session header so multiple updates on the same day stay readable:

```markdown

---

## Session update — HH:MM (local) / YYYY-MM-DD

<new content for this request>
```

Rules for append:

- Preserve all existing text above the separator.
- Use one `---` horizontal rule plus a `## Session update — …` heading per append.
- If the user supplies a session title (e.g. "femto QA"), include it in the heading.
- Do not duplicate the full-day title block (`# 解析ノート YYYY-MM-DD`) in append sections; only the session block is new.
- If an append covers the same topic as an earlier section, add cross-references instead of editing old paragraphs in place.

## New file format (when file does not exist)

Use a single top-level daily title, then standard sections:

```markdown
# 解析ノート YYYY-MM-DD

**解析名:** …
**データ:** …
**作業者:** …

---

## 1. …
…
```

Section names may be English or Japanese; keep the project's existing note style when examples exist under `analysisnote/`.

## Reproducibility requirements

- In "Analyses run" (or equivalent), include:
  - mainconf path
  - jobid
  - configlog path
- Keep `analysisnote/` uncommitted (`.gitignore` managed).

## Notes

- **Default for the same calendar day: append, not overwrite.**
- Overwrite the entire day's file only when the user explicitly requests a full rewrite or refresh from scratch.
- Optional topic-specific notes in the same day may go in separate files under `analysisnote/YYYYMMDD/` (e.g. `femto_phi_proton_phase1.md`) when a session is large; link them from the daily summary.
