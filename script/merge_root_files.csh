#!/bin/tcsh
# Script to merge ROOT files with the same JOBID
# Usage: merge_root_files.csh [-q|--no-progress] [--exclude-list FILE] [--skip-bad-scan] <path_to_root_file>
# Example: merge_root_files.csh /path/to/output_phi_analysis_12345_0.root
# Progress bar is on by default; disable with -q/--no-progress or MERGE_ROOT_PROGRESS=0
# This will merge all files matching: /path/to/output_phi_analysis_12345_*.root
# Output: /path/to/output_phi_analysis_12345_merge.root
#
# Many sub-jobs: argv length is limited by the OS ("Argument list too long").  This script
# merges in rounds of at most MERGE_ROOT_CHUNK_SIZE inputs per intermediate file (default 1000),
# and within each hadd uses batches of MERGE_ROOT_HADD_BATCH paths (default 100) so xargs stays
# under its ~128 KiB command buffer.  Later batches use hadd -f swap OUT subbatch; mv swap OUT
# (ROOT 5 rejects hadd when the target file already exists, even for one input file).
# Within a merge round, independent chunks (up to CHUNK inputs each) can run in parallel
# (MERGE_ROOT_JOBS, default 4).  Each chunk uses its own wk_r*_c* scratch dir for hadd temps.
# Optional env: MERGE_ROOT_CHUNK_SIZE, MERGE_ROOT_HADD_BATCH, MERGE_ROOT_JOBS,
#   MERGE_ROOT_PROGRESS (0|off|no|false disables; default on), MERGE_ROOT_VERBOSE, TMPDIR

set WORKDIR = ""

# Progress bar: on by default; opt out via -q/--no-progress or MERGE_ROOT_PROGRESS=0
set MRF_SHOW_PROGRESS = 1
if ( $?MERGE_ROOT_PROGRESS ) then
    set mrf_prog_env = `echo "$MERGE_ROOT_PROGRESS" | tr '[:upper:]' '[:lower:]'`
    if ( "$mrf_prog_env" == "0" || "$mrf_prog_env" == "off" || "$mrf_prog_env" == "no" || "$mrf_prog_env" == "false" ) then
        set MRF_SHOW_PROGRESS = 0
    endif
endif
set MRF_VERBOSE = 0
if ( $?MERGE_ROOT_VERBOSE ) then
    if ( "$MERGE_ROOT_VERBOSE" != "0" ) set MRF_VERBOSE = 1
endif
set mrf_steps_done = 0
set mrf_steps_total = 0

set CHUNK = 1000
if ( $?MERGE_ROOT_CHUNK_SIZE ) then
    set CHUNK = "$MERGE_ROOT_CHUNK_SIZE"
    expr "$CHUNK" + 0 >& /dev/null
    if ( $status != 0 ) then
        echo "WARNING: MERGE_ROOT_CHUNK_SIZE='$MERGE_ROOT_CHUNK_SIZE' is not an integer; using 1000"
        set CHUNK = 1000
    endif
endif
if ( $CHUNK < 1 ) set CHUNK = 1000

set HADD_BATCH = 100
if ( $?MERGE_ROOT_HADD_BATCH ) then
    set HADD_BATCH = "$MERGE_ROOT_HADD_BATCH"
    expr "$HADD_BATCH" + 0 >& /dev/null
    if ( $status != 0 ) then
        echo "WARNING: MERGE_ROOT_HADD_BATCH='$MERGE_ROOT_HADD_BATCH' is not an integer; using 100"
        set HADD_BATCH = 100
    endif
endif
if ( $HADD_BATCH < 1 ) set HADD_BATCH = 100

set MERGE_JOBS = 4
if ( $?MERGE_ROOT_JOBS ) then
    set MERGE_JOBS = "$MERGE_ROOT_JOBS"
    expr "$MERGE_JOBS" + 0 >& /dev/null
    if ( $status != 0 ) then
        echo "WARNING: MERGE_ROOT_JOBS='$MERGE_ROOT_JOBS' is not an integer; using 4"
        set MERGE_JOBS = 4
    endif
endif
if ( $MERGE_JOBS < 1 ) set MERGE_JOBS = 1

set MRF_MAIN = "$0"
if ( "$MRF_MAIN" !~ /* ) set MRF_MAIN = "`pwd`/${MRF_MAIN}"
set MRF_SCRIPTDIR = `dirname "$MRF_MAIN"`
set MRF_HADD_HELPER = "${MRF_SCRIPTDIR}/merge_root_hadd_from_list.csh"
set MRF_PROGRESS_HELPER = "${MRF_SCRIPTDIR}/merge_root_print_progress.csh"
set MRF_BADSCAN_HELPER = "${MRF_SCRIPTDIR}/scan_bad_subjob_roots.sh"
foreach mrf_helper ( $MRF_HADD_HELPER $MRF_PROGRESS_HELPER )
    if ( ! -x "$mrf_helper" ) then
        if ( -f "$mrf_helper" ) then
            chmod +x "$mrf_helper"
        else
            echo "ERROR: helper not found: $mrf_helper"
            exit 1
        endif
    endif
end

# Parse flags and input .root path
set INPUT_FILE = ""
set EXCLUDE_LIST = ""
set SKIP_BAD_SCAN = 0
foreach mrf_arg ( $argv )
    if ( "$mrf_arg" == "-q" || "$mrf_arg" == "--no-progress" ) then
        set MRF_SHOW_PROGRESS = 0
    else if ( "$mrf_arg" == "--skip-bad-scan" ) then
        set SKIP_BAD_SCAN = 1
    else if ( "$mrf_arg" == "--exclude-list" ) then
        echo "ERROR: --exclude-list requires a file path (use: --exclude-list=FILE)"
        echo "Usage: merge_root_files.csh [-q|--no-progress] [--exclude-list=FILE] [--skip-bad-scan] <path_to_root_file>"
        exit 1
    else if ( "$mrf_arg" =~ --exclude-list=* ) then
        set EXCLUDE_LIST = `echo "$mrf_arg" | sed 's/^--exclude-list=//'`
    else if ( "$mrf_arg" =~ -* ) then
        echo "ERROR: Unknown option: $mrf_arg"
        echo "Usage: merge_root_files.csh [-q|--no-progress] [--exclude-list=FILE] [--skip-bad-scan] <path_to_root_file>"
        exit 1
    else if ( "$INPUT_FILE" == "" ) then
        set INPUT_FILE = "$mrf_arg"
    else
        echo "ERROR: Unexpected extra argument: $mrf_arg"
        echo "Usage: merge_root_files.csh [-q|--no-progress] [--exclude-list=FILE] [--skip-bad-scan] <path_to_root_file>"
        exit 1
    endif
end

if ( "$INPUT_FILE" == "" ) then
    echo "ERROR: No input file specified"
    echo "Usage: merge_root_files.csh [-q|--no-progress] [--exclude-list=FILE] [--skip-bad-scan] <path_to_root_file>"
    echo "Example: merge_root_files.csh /path/to/X_prefix_Y_JOBID_SUBID.root"
    exit 1
endif

# Check if input file exists
if ( ! -f "$INPUT_FILE" ) then
    echo "ERROR: Input file does not exist: $INPUT_FILE"
    exit 1
endif

# Extract directory and filename
set FILE_DIR = `dirname "$INPUT_FILE"`
set FILE_NAME = `basename "$INPUT_FILE"`

# Check if file has .root extension
set FILE_EXT = `echo "$FILE_NAME" | sed 's/.*\.//'`
if ( "$FILE_EXT" != "root" ) then
    echo "ERROR: Input file must have .root extension: $INPUT_FILE"
    exit 1
endif

# Extract base pattern (X_..._Y_JOBID) by removing _SUBID.root
set FILE_BASE = `echo "$FILE_NAME" | sed 's/\.root$//'`
set PATTERN_BASE = `echo "$FILE_BASE" | sed 's/_[^_]*$//'`

if ( "$PATTERN_BASE" == "" ) then
    echo "ERROR: Could not extract pattern from filename: $FILE_NAME"
    echo "Expected format: X_..._Y_JOBID_SUBID.root"
    exit 1
endif

# Construct search pattern and output filename
set SEARCH_PATTERN = "${PATTERN_BASE}_*.root"
set MERGE_NAME = "${PATTERN_BASE}_merge.root"
set OUTPUT_FILE = "${FILE_DIR}/${MERGE_NAME}"

echo "=========================================="
echo "Merging ROOT files"
echo "Input pattern: ${FILE_DIR}/${SEARCH_PATTERN}"
echo "Output file: $OUTPUT_FILE"
echo "Max inputs per intermediate hadd: $CHUNK (MERGE_ROOT_CHUNK_SIZE)"
echo "Paths per hadd batch: $HADD_BATCH (MERGE_ROOT_HADD_BATCH)"
echo "Parallel chunk jobs: $MERGE_JOBS (MERGE_ROOT_JOBS, 1 = serial)"
if ( $MRF_SHOW_PROGRESS ) then
    echo "Progress bar: on (use -q or MERGE_ROOT_PROGRESS=0 to disable)"
else
    echo "Progress bar: off"
endif
echo "=========================================="

# Check if hadd command is available
which hadd >& /dev/null
if ( $status != 0 ) then
    echo "ERROR: hadd command not found. Please setup ROOT environment."
    exit 1
endif

set MRF_TMP = /tmp
if ( $?TMPDIR ) then
    if ( "$TMPDIR" != "" ) set MRF_TMP = "$TMPDIR"
endif
set WORKDIR = `mktemp -d "${MRF_TMP}/merge_root.XXXXXX"`
if ( $status != 0 || "$WORKDIR" == "" || ! -d "$WORKDIR" ) then
    echo "ERROR: Could not create temporary directory (mktemp). Try setting TMPDIR."
    set WORKDIR = ""
    exit 1
endif

onintr cleanup_interrupt

# Exclude previous merge product so re-runs do not pull in *_merge.root
find "$FILE_DIR" -maxdepth 1 -name "$SEARCH_PATTERN" -type f ! -name "$MERGE_NAME" | sort > "$WORKDIR/list_r0_all.txt"

if ( "$EXCLUDE_LIST" == "" && $SKIP_BAD_SCAN == 0 ) then
    if ( ! -x "$MRF_BADSCAN_HELPER" ) then
        if ( -f "$MRF_BADSCAN_HELPER" ) then
            chmod +x "$MRF_BADSCAN_HELPER"
        else
            echo "ERROR: helper not found: $MRF_BADSCAN_HELPER"
            goto cleanup_fail
        endif
    endif
    set AUTO_EXCLUDE = "${WORKDIR}/auto_exclude.txt"
    echo "Running bad-root scan helper: $MRF_BADSCAN_HELPER"
    "$MRF_BADSCAN_HELPER" --sample "$INPUT_FILE" --output "$AUTO_EXCLUDE"
    if ( $status != 0 ) then
        echo "ERROR: bad-root scan failed"
        goto cleanup_fail
    endif
    set EXCLUDE_LIST = "$AUTO_EXCLUDE"
endif

if ( "$EXCLUDE_LIST" != "" ) then
    if ( ! -f "$EXCLUDE_LIST" ) then
        echo "ERROR: Exclude list not found: $EXCLUDE_LIST"
        goto cleanup_fail
    endif
    awk 'NF{print $0}' "$EXCLUDE_LIST" | sort -u >! "${WORKDIR}/exclude_norm.txt"
    set EXCLUDE_COUNT = `wc -l < "${WORKDIR}/exclude_norm.txt" | awk '{print $1}'`
    echo "Applying exclude list: $EXCLUDE_LIST (entries: $EXCLUDE_COUNT)"
    if ( $EXCLUDE_COUNT > 0 ) then
        grep -F -x -v -f "${WORKDIR}/exclude_norm.txt" "${WORKDIR}/list_r0_all.txt" >! "$WORKDIR/list_r0.txt"
    else
        cp -f "${WORKDIR}/list_r0_all.txt" "$WORKDIR/list_r0.txt"
    endif
else
    cp -f "${WORKDIR}/list_r0_all.txt" "$WORKDIR/list_r0.txt"
endif

set FILE_COUNT = `wc -l < "$WORKDIR/list_r0.txt" | awk '{print $1}'`
if ( "$FILE_COUNT" == "" || $FILE_COUNT < 1 ) then
    echo "ERROR: No files left to merge after exclusions: ${FILE_DIR}/${SEARCH_PATTERN}"
    goto cleanup_fail
endif

echo "Found $FILE_COUNT file(s) to merge."
if ( $FILE_COUNT <= 30 ) then
    foreach f ( `cat "$WORKDIR/list_r0.txt"` )
        echo "  - $f"
    end
else
    echo "  (listing suppressed; see $WORKDIR/list_r0.txt)"
    head -5 "$WORKDIR/list_r0.txt" | sed 's/^/  - /'
    echo "  - ..."
endif

if ( -f "$OUTPUT_FILE" ) then
    echo ""
    echo "WARNING: Output file already exists: $OUTPUT_FILE"
    echo "It will be overwritten (hadd -f on every stage)."
    echo ""
endif

# Estimate total hadd steps (chunk jobs in each round + one final merge)
set mrf_n = $FILE_COUNT
while ( $mrf_n > $CHUNK )
    set mrf_nc = `expr \( $mrf_n + $CHUNK - 1 \) / $CHUNK`
    @ mrf_steps_total = $mrf_steps_total + $mrf_nc
    set mrf_n = $mrf_nc
end
@ mrf_steps_total = $mrf_steps_total + 1
if ( $MRF_SHOW_PROGRESS ) then
    echo "Planned merge steps: $mrf_steps_total (chunk hadd stages + final merge)"
    echo ""
endif

set round = 0
set CUR = "$WORKDIR/list_r0.txt"

merge_round:
if ( ! -f "$CUR" ) then
    echo "ERROR: File list missing at merge round ${round}: $CUR"
    goto cleanup_fail
endif

set nlines = `wc -l < "$CUR" | awk '{print $1}'`
if ( "$nlines" == "" || $nlines < 1 ) then
    echo "ERROR: Empty file list at $CUR"
    goto cleanup_fail
endif

if ( $nlines <= $CHUNK ) then
    echo ""
    echo "Merging $nlines file(s) into final output..."
    if ( $MRF_SHOW_PROGRESS ) then
        set mrf_prog_detail = "final hadd ($nlines files)"
        set mrf_prog_chunks_done = 0
        set mrf_prog_chunks_total = 1
        tcsh -f "$MRF_PROGRESS_HELPER" "$mrf_steps_done" "$mrf_steps_total" "$round" "$mrf_prog_chunks_done" "$mrf_prog_chunks_total" "$mrf_prog_detail"
    endif
    tcsh -f "$MRF_HADD_HELPER" "$OUTPUT_FILE" "$CUR" "$WORKDIR" "$HADD_BATCH"
    if ( $status != 0 ) goto cleanup_fail
    if ( $MRF_SHOW_PROGRESS ) then
        @ mrf_steps_done = $mrf_steps_total
        set mrf_prog_detail = "done"
        set mrf_prog_chunks_done = 1
        set mrf_prog_chunks_total = 1
        tcsh -f "$MRF_PROGRESS_HELPER" "$mrf_steps_done" "$mrf_steps_total" "$round" "$mrf_prog_chunks_done" "$mrf_prog_chunks_total" "$mrf_prog_detail"
    endif
    goto merge_done
endif

@ next_round = $round + 1
set NEXT = "${WORKDIR}/list_r${next_round}.txt"
rm -f "$NEXT"
touch "$NEXT"

set chunk_id = 0
set start = 1
echo ""
echo "Merge round ${round}: splitting $nlines file(s) into chunks of up to $CHUNK ..."

# Pass 1: build per-chunk input lists
while ( $start <= $nlines )
    @ end = $start + $CHUNK - 1
    if ( $end > $nlines ) @ end = $nlines
    set MRF_CLIST = "${WORKDIR}/chunk_r${round}_c${chunk_id}.lst"
    sed -n "${start},${end}p" "$CUR" | awk 'NF {print}' >! "$MRF_CLIST"
    set MRF_N = `wc -l < "$MRF_CLIST" | awk '{print $1}'`
    if ( "$MRF_N" == "" || $MRF_N < 1 ) then
        echo "ERROR: empty chunk list for lines ${start}-${end} in $CUR"
        goto cleanup_fail
    endif
    @ chunk_id = $chunk_id + 1
    @ start = $end + 1
end
set nchunks = $chunk_id

if ( $nchunks < 1 ) then
    echo "ERROR: no chunks planned for merge round ${round}"
    goto cleanup_fail
endif

# Pass 2: run chunk hadd (parallel batches of MERGE_JOBS)
set mrf_prog_chunks_total = $nchunks
set mrf_prog_chunks_done = 0
if ( $MRF_SHOW_PROGRESS ) then
    set mrf_prog_detail = "round ${round} starting (${nchunks} chunks)"
    tcsh -f "$MRF_PROGRESS_HELPER" "$mrf_steps_done" "$mrf_steps_total" "$round" "$mrf_prog_chunks_done" "$mrf_prog_chunks_total" "$mrf_prog_detail"
endif
set launched = 0
while ( $launched < $nchunks )
    set np = 0
    set batch_bg = 0
    while ( $np < $MERGE_JOBS && $launched < $nchunks )
        set cid = $launched
        set PART = "${WORKDIR}/r${round}_c${cid}.root"
        set MRF_CLIST = "${WORKDIR}/chunk_r${round}_c${cid}.lst"
        set CHUNK_WKDIR = "${WORKDIR}/wk_r${round}_c${cid}"
        mkdir -p "$CHUNK_WKDIR"
        if ( $MRF_VERBOSE ) then
            set chunk_n = `wc -l < "$MRF_CLIST" | awk '{print $1}'`
            echo "  Chunk ${cid}: ${chunk_n} file(s) -> $PART"
        endif
        if ( $MERGE_JOBS > 1 ) then
            ( tcsh -f "$MRF_HADD_HELPER" "$PART" "$MRF_CLIST" "$CHUNK_WKDIR" "$HADD_BATCH" >& "${CHUNK_WKDIR}/log" ; echo $? > "${CHUNK_WKDIR}/exit" ) &
            set batch_bg = 1
        else
            tcsh -f "$MRF_HADD_HELPER" "$PART" "$MRF_CLIST" "$CHUNK_WKDIR" "$HADD_BATCH" >& "${CHUNK_WKDIR}/log"
            echo $? > "${CHUNK_WKDIR}/exit"
        endif
        @ launched++
        @ np++
    end
    # tcsh/csh: wait takes no arguments (wait $pid => "Too many arguments")
    if ( $batch_bg ) wait
    set mrf_prog_chunks_done = $launched
    @ mrf_steps_done = $mrf_steps_done + $np
    if ( $MRF_SHOW_PROGRESS ) then
        set mrf_prog_detail = "round ${round} batch finished (${np} chunk(s))"
        tcsh -f "$MRF_PROGRESS_HELPER" "$mrf_steps_done" "$mrf_steps_total" "$round" "$mrf_prog_chunks_done" "$mrf_prog_chunks_total" "$mrf_prog_detail"
    endif
end

# Pass 3: verify chunks and write intermediate list (chunk order)
set cid = 0
while ( $cid < $nchunks )
    set PART = "${WORKDIR}/r${round}_c${cid}.root"
    set CHUNK_WKDIR = "${WORKDIR}/wk_r${round}_c${cid}"
    if ( ! -f "${CHUNK_WKDIR}/exit" ) then
        echo "ERROR: chunk ${cid} did not finish (no exit file in $CHUNK_WKDIR)"
        goto cleanup_fail
    endif
    set chunk_st = `cat "${CHUNK_WKDIR}/exit"`
    if ( "$chunk_st" != "0" ) then
        echo "ERROR: hadd failed on chunk ${cid} in merge round ${round} (exit $chunk_st)"
        echo "  See ${CHUNK_WKDIR}/log"
        goto cleanup_fail
    endif
    if ( ! -f "$PART" ) then
        echo "ERROR: chunk output missing: $PART"
        goto cleanup_fail
    endif
    printf '%s\n' "$PART" >> "$NEXT"
    @ cid++
end

if ( ! -f "$NEXT" ) then
    echo "ERROR: Intermediate list was not created: $NEXT"
    goto cleanup_fail
endif

set MRF_N = `wc -l < "$NEXT" | awk '{print $1}'`
if ( "$MRF_N" == "" || $MRF_N < 1 ) then
    echo "ERROR: Intermediate list is empty: $NEXT"
    goto cleanup_fail
endif

set round = $next_round
set CUR = "$NEXT"
goto merge_round

merge_done:
if ( ! -f "$OUTPUT_FILE" ) then
    echo "ERROR: Output file was not created: $OUTPUT_FILE"
    goto cleanup_fail
endif

set OUTPUT_SIZE = `ls -lh "$OUTPUT_FILE" | awk '{print $5}'`
rm -rf "$WORKDIR"
set WORKDIR = ""
onintr -

echo ""
echo "=========================================="
echo "Merge completed successfully!"
echo "Output file: $OUTPUT_FILE"
echo "Output size: $OUTPUT_SIZE"
echo "=========================================="

exit 0

cleanup_interrupt:
onintr -
echo ""
echo "Interrupted; removing scratch directory."
if ( "$WORKDIR" != "" && -d "$WORKDIR" ) rm -rf "$WORKDIR"
exit 130

cleanup_fail:
onintr -
if ( "$WORKDIR" != "" && -d "$WORKDIR" ) rm -rf "$WORKDIR"
exit 1
