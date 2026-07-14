#!/bin/tcsh
# Merge ROOT files listed one-per-line in LIST into OUT (batched for xargs / ROOT 5).
# Usage: merge_root_hadd_from_list.csh OUTFILE LISTFILE WORKDIR [HADD_BATCH]
# Exit 0 on success, 1 on failure.

if ( $#argv < 3 ) then
    echo "Usage: merge_root_hadd_from_list.csh OUTFILE LISTFILE WORKDIR [HADD_BATCH]"
    exit 1
endif

set mrf_out = "$1"
set mrf_list = "$2"
set WORKDIR = "$3"
set HADD_BATCH = 100
if ( $#argv >= 4 ) set HADD_BATCH = "$4"
expr "$HADD_BATCH" + 0 >& /dev/null
if ( $status != 0 || $HADD_BATCH < 1 ) set HADD_BATCH = 100

if ( ! -f "$mrf_list" ) then
    echo "ERROR: list file missing: $mrf_list"
    exit 1
endif

which hadd >& /dev/null
if ( $status != 0 ) then
    echo "ERROR: hadd not found"
    exit 1
endif

set mrf_swap = "${WORKDIR}/hadd_swap.root"
set mrf_total = `wc -l < "$mrf_list" | awk '{print $1}'`
if ( "$mrf_total" == "" || $mrf_total < 1 ) then
    echo "ERROR: No paths in $mrf_list"
    exit 1
endif

set mrf_pos = 1
set mrf_first = 1
while ( $mrf_pos <= $mrf_total )
    @ mrf_end = $mrf_pos + $HADD_BATCH - 1
    if ( $mrf_end > $mrf_total ) @ mrf_end = $mrf_total
    set mrf_blst = "$WORKDIR/hadd_b_${mrf_pos}_${mrf_end}.lst"
    sed -n "${mrf_pos},${mrf_end}p" "$mrf_list" | awk 'NF {print}' >! "$mrf_blst"
    set mrf_bn = `wc -l < "$mrf_blst" | awk '{print $1}'`
    if ( "$mrf_bn" != "" && $mrf_bn >= 1 ) then
        if ( $mrf_first ) then
            rm -f "$mrf_out"
            xargs -r hadd -f "$mrf_out" < "$mrf_blst"
            set mrf_first = 0
            if ( $status != 0 || ! -f "$mrf_out" ) then
                echo "ERROR: hadd failed for first batch lines ${mrf_pos}-${mrf_end} into $mrf_out (status $status)"
                exit 1
            endif
        else
            set mrf_bpart = "${WORKDIR}/b_${mrf_pos}_${mrf_end}.root"
            rm -f "$mrf_bpart"
            xargs -r hadd -f "$mrf_bpart" < "$mrf_blst"
            if ( $status != 0 || ! -f "$mrf_bpart" ) then
                echo "ERROR: hadd failed building sub-batch ${mrf_pos}-${mrf_end} (status $status)"
                exit 1
            endif
            rm -f "$mrf_swap"
            hadd -f "$mrf_swap" "$mrf_out" "$mrf_bpart"
            if ( $status != 0 || ! -f "$mrf_swap" ) then
                echo "ERROR: hadd failed merging sub-batch ${mrf_pos}-${mrf_end} into $mrf_out (status $status)"
                rm -f "$mrf_bpart" "$mrf_swap"
                exit 1
            endif
            mv -f "$mrf_swap" "$mrf_out"
            if ( $status != 0 || ! -f "$mrf_out" ) then
                echo "ERROR: could not replace $mrf_out after sub-batch ${mrf_pos}-${mrf_end}"
                rm -f "$mrf_bpart" "$mrf_swap"
                exit 1
            endif
            rm -f "$mrf_bpart"
        endif
    endif
    @ mrf_pos = $mrf_end + 1
end

if ( $mrf_first ) then
    echo "ERROR: nothing merged into $mrf_out"
    exit 1
endif

exit 0
