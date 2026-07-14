#!/bin/tcsh
# Print one-line merge progress bar.
# Usage: merge_root_print_progress.csh DONE TOTAL ROUND CHUNKS_DONE CHUNKS_TOTAL "detail message"

if ( $#argv < 6 ) then
    echo "Usage: merge_root_print_progress.csh DONE TOTAL ROUND CHUNKS_DONE CHUNKS_TOTAL detail"
    exit 1
endif

set mrf_steps_done = "$1"
set mrf_steps_total = "$2"
set round = "$3"
set mrf_prog_chunks_done = "$4"
set mrf_prog_chunks_total = "$5"
set mrf_prog_detail = "$6"

set mrf_pct = 0
set mrf_fill = 0
if ( $mrf_steps_total > 0 ) then
    set mrf_pct = `expr 100 \* $mrf_steps_done / $mrf_steps_total`
    set mrf_fill = `expr 40 \* $mrf_steps_done / $mrf_steps_total`
endif
if ( $mrf_fill > 40 ) set mrf_fill = 40
set mrf_bar = ""
set mrf_bi = 0
while ( $mrf_bi < $mrf_fill )
    set mrf_bar = "${mrf_bar}#"
    @ mrf_bi++
end
while ( $mrf_bi < 40 )
    set mrf_bar = "${mrf_bar}-"
    @ mrf_bi++
end
echo "[${mrf_bar}] ${mrf_steps_done}/${mrf_steps_total} (${mrf_pct}%)  round ${round}  chunks ${mrf_prog_chunks_done}/${mrf_prog_chunks_total}  ${mrf_prog_detail}"

exit 0
