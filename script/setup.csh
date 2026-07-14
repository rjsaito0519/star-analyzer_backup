#!/usr/bin/env tcsh
# Source from project root: source ./script/setup.csh MAINCONF_PATH
# For bash/zsh use: source ./script/setup.sh MAINCONF_PATH

if ( "$#argv" < 1 ) then
  echo "Usage: source ./script/setup.csh MAINCONF_PATH" >& /dev/stderr
  return 1
endif

set PROJECT_ROOT="$cwd"
if ( ! -f "$PROJECT_ROOT/script/analysis_info_helper.py" ) then
  echo "ERROR: run 'source ./script/setup.csh ...' from the project root." >& /dev/stderr
  return 1
endif

set MAINCONF="$1"
setenv STAR_ANA_MAINCONF "$MAINCONF"
echo "$MAINCONF" > "$PROJECT_ROOT/.current_mainconf"
set LIBRARY_TAG=`cd "$PROJECT_ROOT" && python script/analysis_info_helper.py --library-tag --mainconf "$MAINCONF"`
if ( $status != 0 ) then
  return 1
endif
set LIBRARY_TAG=`echo "$LIBRARY_TAG" | xargs`

which starver >& /dev/null
if ( $status != 0 ) then
  echo "ERROR: starver command not found. Load STAR environment first (for this project, run 'sl7' before setup/make)." >& /dev/stderr
  return 1
endif

starver "$LIBRARY_TAG"
if ( $status != 0 ) then
  return 1
endif

set STAR_TAG_DIR="/star/nfs4/AFS/star/packages/$LIBRARY_TAG"
if ( -d "$STAR_TAG_DIR" ) then
  setenv STAR "$STAR_TAG_DIR"
endif

set STAR_HOST_SYS_CANDIDATES=()
set STAR_HOST_SYS_CANDIDATES=( sl73_x8664_gcc485 sl74_x8664_gcc485 )
if ( $?STAR_HOST_SYS ) then
  set STAR_HOST_SYS_CANDIDATES=( $STAR_HOST_SYS_CANDIDATES $STAR_HOST_SYS )
endif

set RESOLVED_STAR_HOST_SYS=""
foreach candidate ( $STAR_HOST_SYS_CANDIDATES sl73_gcc485 sl74_gcc485 )
  if ( "$candidate" != "" && -d "$STAR/.$candidate" ) then
    set RESOLVED_STAR_HOST_SYS="$candidate"
    break
  endif
end

if ( "$RESOLVED_STAR_HOST_SYS" == "" ) then
  echo "ERROR: could not resolve STAR_HOST_SYS under $STAR." >& /dev/stderr
  return 1
endif

setenv STAR_HOST_SYS "$RESOLVED_STAR_HOST_SYS"
setenv STAR_BIN "${STAR}/.${STAR_HOST_SYS}/bin"
setenv STAR_LIB "${STAR}/.${STAR_HOST_SYS}/lib"

set ROOT_ARCH_DIR="linux-rhel7-x86"
if ( "$STAR_HOST_SYS" =~ *x8664* ) then
  set ROOT_ARCH_DIR="linux-rhel7-x86_64"
endif

set saved_nonomatch=$?nonomatch
set nonomatch
set ROOT_BIN_CANDIDATES=( /cvmfs/star.sdcc.bnl.gov/star-spack/spack/opt/spack/${ROOT_ARCH_DIR}/gcc-4.8.5/root-*/bin )
if ( ! $saved_nonomatch ) then
  unset nonomatch
endif
set ROOT_BIN=""
if ( $#ROOT_BIN_CANDIDATES > 0 && -d "$ROOT_BIN_CANDIDATES[1]" ) then
  set ROOT_BIN="$ROOT_BIN_CANDIDATES[1]"
endif

setenv PATH "${STAR}/mgr:${STAR_BIN}:${PATH}"
if ( "$ROOT_BIN" != "" ) then
  setenv PATH "${ROOT_BIN}:${PATH}"
endif

if ( $?LD_LIBRARY_PATH ) then
  setenv LD_LIBRARY_PATH "${STAR_LIB}:${LD_LIBRARY_PATH}"
else
  setenv LD_LIBRARY_PATH "${STAR_LIB}"
endif

echo "LIBRARY_TAG: $LIBRARY_TAG"
echo "STAR: $STAR"
echo "STAR_HOST_SYS: $STAR_HOST_SYS"

which root-config >& /dev/null
if ( $status == 0 ) then
  set ROOT_CFLAGS=`root-config --cflags`
  if ( "$STAR_HOST_SYS" =~ *x8664* && "$ROOT_CFLAGS" =~ *-m32* ) then
    echo "WARNING: root-config still points to 32-bit ROOT: `which root-config`" >& /dev/stderr
  endif
endif
