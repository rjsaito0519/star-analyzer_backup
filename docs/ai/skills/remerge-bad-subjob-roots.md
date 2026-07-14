# Skill: remerge-bad-subjob-roots

Rebuild a merge output while excluding bad subjob ROOT files.

## Use when

- `*_merge.root` is suspiciously small.
- `hadd` fails during merge.
- QA plots indicate near-empty merge content.

## Workflow

1. Identify `anaName` and `jobid`, then choose a sample file:
   - `rootfile/<anaName>/<anaName>_<jobid>_0.root`
2. Generate exclusion list:
   ```bash
   ./script/scan_bad_subjob_roots.sh \
     --sample rootfile/<anaName>/<anaName>_<jobid>_0.root \
     --output /tmp/bad_subjob_roots_<anaName>_<jobid>.txt
   ```
3. Re-merge with explicit exclusion list:
   ```bash
   ./script/merge_root_files.csh \
     --exclude-list=/tmp/bad_subjob_roots_<anaName>_<jobid>.txt \
     rootfile/<anaName>/<anaName>_<jobid>_0.root
   ```
4. Validate merge result:
   - Confirm output path: `rootfile/<anaName>/<anaName>_<jobid>_merge.root`
   - Check output size/logs for abnormal shrink.
   - Re-run checkHist QA for the analysis.

## Watch-merge path

For rerun through watcher:

```bash
./script/watch_job_and_merge.sh \
  --runmeta job/run/runmeta/runmeta_<anaName>_<jobid>.json \
  --force-merge \
  --exclude-bad-roots /tmp/bad_subjob_roots_<anaName>_<jobid>.txt
```

## Notes

- `merge_root_files.csh` runs bad-root scan automatically by default.
- Use `--skip-bad-scan` only when scan is intentionally disabled.
- Keep exclusion list as absolute paths, one file per line.
