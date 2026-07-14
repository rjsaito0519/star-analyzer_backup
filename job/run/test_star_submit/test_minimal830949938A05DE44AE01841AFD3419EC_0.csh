#!/bin/csh
# -------------------------------------- 
# Script generated on Tue Mar 10 04:48:33 EDT 2026 by the Simple Unified Meta Scheduler 1.10.50 and submitted with:
# cd /star/u/oura/work/star-analysis/job/run/test_star_submit; condor_submit -verbose /star/u/oura/work/star-analysis/job/run/test_star_submit/test_minimal830949938A05DE44AE01841AFD3419EC_0.condor
# --------------------------------------



if ( $?SUMS_EXECUTION ) then
    goto USERCODESECTION
endif





/bin/echo 'We are starting on node: '`/bin/hostname`


# Preparing environment variables
ENVSETUPSECTION:

setenv EVENTS_START "-1"
setenv EVENTS_STOP "-1"
setenv FILEBASENAME "st_physics_20056032_raw_0000001"
setenv FILELIST "/star/u/oura/work/star-analysis/job/run/test_star_submit/test_minimal830949938A05DE44AE01841AFD3419EC_0.list"
setenv FILELIST_ALL "/star/u/oura/work/star-analysis/job/run/test_star_submit/test_minimal830949938A05DE44AE01841AFD3419EC.list"
setenv INPUTFILECOUNT "1"
setenv JOBID "830949938A05DE44AE01841AFD3419EC_0"
setenv JOBINDEX "0"
setenv LOGGING "STD"
setenv REQUESTID "830949938A05DE44AE01841AFD3419EC"
setenv SUBMITATTEMPT "1"
setenv SUBMITTINGDIRECTORY "/star/u/oura/work/star-analysis/job/run/test_star_submit"
setenv SUBMITTINGNODE "rcas6016.rcf.bnl.gov"
setenv SUBMIT_TIME "2026-03-10 08:48:33"
setenv SUMS_AUTHENTICATED_USER "oura@rhic.bnl.gov"
setenv SUMS_USER "oura"
setenv SUMS_nProcesses "1"
setenv SUMS_name "test_minimal"
setenv USEXROOTD "1"
setenv INPUTFILE0 "/star/data107/reco/production_19GeV_2019/ReversedFullField/P24iy/2019/056/20056032/st_physics_20056032_raw_0000001.picoDst.root"

# This block is used by CONDOR where $HOME in not set

if ( ! $?USER ) then
    echo "USER is not defined"
    set USER=`id | sed "s/).*//" | sed "s/.*(//"`
endif
if ( ! $?HOME ) then
    echo "HOME is not defined"

    if ( -x /usr/bin/getent ) then
	# we have getent, should not be on aix, bsd, Tru64 however
	# will work for Linux
	echo "Using getent method"
	setenv HOME `/usr/bin/getent passwd $USER | /bin/sed 's|.*\:.*\:.*\:.*\:\([^\:]*\):.*|\1|'`
    else 
	set PTEST=`which perl`
	if ( "$PTEST" != "" ) then
	    echo "Using perl method"
	    # we have perl defined, we can get info from there
	    /bin/cat <<EOF >test$$.pl
my(\$user) = getpwuid(\$<);
@items = getpwnam(\$user);
print \$items[7];
EOF
	    setenv HOME `$PTEST test$$.pl` && /bin/rm  -f test$$.pl
	else
	    set CTEST=`which cc`
	    if ( "$CTEST" != "" ) then
		echo "Using C code method"
		# use C code for doing this
		/bin/cat <<EOF >test$$.c
#include <stdio.h>
#include <unistd.h>
#include <pwd.h>


int main()
{
  struct passwd *info;
  uid_t          uid;

  /* get process UID */
  uid = getuid();
  info= getpwuid(uid);

  (void) printf("%s\n",info->pw_dir);
  return 0;
} 

EOF
		$CTEST -o test$$ test$$.c
		/bin/chmod +x test$$
		setenv HOME `./test$$` && /bin/rm -f test$$ test$$.c
	    else
		echo "We have no ways to define HOME and it is not defined"
		exit
	    endif
	endif
    endif
endif

echo "HOME is now $HOME"


# Default value for path if not defined.
if ( ! $?PATH ) then
   setenv PATH /usr/local/bin:/bin:/usr/bin
endif


/usr/bin/test -r $HOME/.cshrc && source $HOME/.cshrc

# Creating the scratch directory, return failure status
set SUMS_try=0
set SUMS_PAD=""

MKDTRY:
setenv SCRATCH "/tmp/$USER$SUMS_PAD/$JOBID"
/bin/mkdir -p $SCRATCH   >& /dev/null
set STS=$status
if (! -d $SCRATCH) then
       #test if porper UID
       set SUMS_IsUID=`(/usr/bin/test -O /tmp/$USER$SUMS_PAD && echo 1) || echo 0`
       if ( $SUMS_try == 0 &&  -e "/tmp/$USER$SUMS_PAD" && ! $SUMS_IsUID) then
            # First try, directory exists but is not owned by $USER
            # Create a different path and try again
            echo "Scheduler:: $SCRATCH not owned by $USER, trying alternative"
            @ try++
            set SUMS_seed=`/bin/date +%j%H%M%S`
            set SUMS_PAD=`expr $SUMS_seed  \* 25973 \% 100`
            goto MKDTRY
        else
		 echo "Scheduler:: Failed to create $SCRATCH on $HOST"
	         exit $STS
        endif

endif


##########################################################
# Detached script pid so that signals will propagate to  #
# child processes such as root4star, giving a chance to  #
# close out open files.                                  #
##########################################################
if ( ! $?SUMS_EXECUTION ) then
    echo "$$ We will spawn ourselves ($0 $*)"
    echo "$$ We BEGIN on [`/bin/date`]"

    onintr SUMS_SIGHANDLER
    ((setenv SUMS_EXECUTION 1 && nohup $0 $*) > $SCRATCH/.output.log ) >& $SCRATCH/.errror.log & 
    set PID=$!

    echo "$$ We are waiting for $PID "
    set loop=0
    set DELAY=1
    set CHLDPID=""
    set CHKP=10
    while (`/bin/ps --no-headers -p $PID` != "")
       if ( $loop % $CHKP == 0 ) then
           # check this only once every CHKP*DELAY seconds
           set GTEST=`/bin/ps -efH | /bin/grep -e "^[a-z]\+ \+[0-9]\+ \+$PID .*" | /bin/grep -v $$ |  /bin/grep -v grep | /usr/bin/head -1 | /usr/bin/awk '{print $2}'`
           if ( "$GTEST" != "" && "$GTEST" != "$CHLDPID") then
               echo "$$ Grandchild PID is $GTEST - found in $loop x $DELAY seconds"
               if ( "$CHLDPID" == "") then
                   echo "$$ Establishing first watcher   on    `/bin/date`"
                   set DELAY=2
               else
                   echo "$$ Grandchild to watch has changed on `/bin/date`"
               endif
               set CHLDPID="$GTEST"
               echo "$$ Process tree is $$ (main) -> $PID (child) -> $CHLDPID (grandchild)"
               /bin/ps -l $CHLDPID
            endif
       endif
       @ loop++
       sleep $DELAY
   end
   cat $SCRATCH/.output.log >> /dev/stdout
    cat $SCRATCH/.errror.log >> /dev/stderr

# Delete the scratch directory
    /bin/rm -fr $SCRATCH
    echo "$$ Execution of ($0 $*) has ended, exiting normally"
    echo "$$ We END on [`/bin/date`]"
    exit 0

    SUMS_SIGHANDLER:
        echo "$$ We received a signal - sending TERM to PID=$PID group"
        kill -TERM -$PID
        echo "$$ We have sent an TERM signal to $PID and waiting for $CHLDPID on `/bin/date`"
        while (`/bin/ps --no-headers -p $CHLDPID` != "")
           sleep 2
        end
        echo "$$ Dumping the output/error channels ---->"
        cat $SCRATCH/.output.log >> /dev/stdout
        cat $SCRATCH/.errror.log >> /dev/stderr
        echo "$$ <---- done with output/error channels"
        echo "$$ We are now leaving with error on [`/bin/date`]"
        exit 1
endif

USERCODESECTION:
echo "$$ Stepping into the main SUMS wrapper program with [$*]"




#import packages 
echo '.... building sandbox'
setenv pwd `pwd` 
foreach object ( `find /star/u/oura/work/star-analysis/ -maxdepth 1 -name 'README.md' ` ) 
 
 
          if($object != '/star/u/oura/work/star-analysis/') then 
 
                 foreach DIRObject ( `find $object`) 
 
                        setenv NewLink  $SCRATCH/`echo $DIRObject  | /bin/sed 's|/star/u/oura/work/star-analysis/||'` 
 
                        if(! -e $NewLink) then 
                             if(-f $DIRObject) then 
                                   /bin/ln -s  `echo $DIRObject | /bin/sed "s|\(^[^/]\)\(.*\)|$pwd/\1\2|"`  $NewLink 
                             else 
                                   /bin/mkdir -p   $NewLink 
                             endif 
                        endif 
               end 
 
 
        endif 
 
end 


# Used for cleanning up the sandbox later
/bin/ls $SCRATCH > ${SCRATCH}/.sandboxFiles
echo ".sandboxFiles" >> ${SCRATCH}/.sandboxFiles



#Note: The default directory in which jobs start has been fix to $SCRATCH
cd $SCRATCH

#--------------- Copy input to SCRATCH --------------------

setenv LocalInputFileDir ${SCRATCH}/INPUTFILES
mkdir ${LocalInputFileDir}

setenv NewFILELIST ${LocalInputFileDir}/${JOBID}.local.list
touch $NewFILELIST

set XRootCopySuccessCount=0
set XRootCopyCount=0
set FileIndex=0
foreach RemoteFile ( `cat ${FILELIST} | awk '{printf($1"\n")}'` )
        set RemoteFileWithoutEvents=${RemoteFile}
        set RemoteFile=`cat ${FILELIST} | grep ${RemoteFile}`
        set nEvents=`echo ${RemoteFile} | awk '{printf($2"\n")}'`
        set isRootFile=`echo ${RemoteFile} | grep -e "^[Rr]oot" | wc -l`
        set isS3File=`echo ${RemoteFile} | grep -e "^[Ss]3:" | wc -l`

        if(${isS3File}) then
                   set LocalFile=${RemoteFile}
        else if(${isRootFile}) then
                  @ XRootCopyCount++
                  set LocalFile=${LocalInputFileDir}/`echo ${RemoteFileWithoutEvents} | sed 's|\(.*/\)\([^\]*\)$|\2|g'`
                  echo running: xrdcp --retry 3 ${RemoteFileWithoutEvents} ${LocalFile} \>\& /dev/null
                  xrdcp --retry 3 ${RemoteFileWithoutEvents} ${LocalFile} >& /dev/null
                  echo "xrdcp copy done: `/bin/date`"
                  if( -e ${LocalFile} ) then
                        @ XRootCopySuccessCount++
                        echo "${LocalFile} ${nEvents}" >> $NewFILELIST
                  else
                        set LocalFile="${RemoteFile}"
                        echo ${LocalFile} >> $NewFILELIST
                  endif
        else
                set LocalFile=${LocalInputFileDir}/`echo ${RemoteFileWithoutEvents} | sed 's|\(.*/\)\([^\]*\)$|\2|g'`
                echo Running: cp ${RemoteFileWithoutEvents}  ${LocalFile}
                cp ${RemoteFileWithoutEvents}  ${LocalFile}
                if( -e ${LocalFile} ) then
                        echo "${LocalFile} ${nEvents}" >> $NewFILELIST
                else
                        echo ${RemoteFile} >> $NewFILELIST
                        set LocalFile="${RemoteFile}"
                endif
        endif

        set LocalFileWithoutEvents=`echo ${LocalFile} | awk '{printf($1"\n")}'`
        setenv INPUTFILE${FileIndex} ${LocalFileWithoutEvents}
        @ FileIndex++
end

echo Swapping \$FILELIST with: ${NewFILELIST}
setenv FILELIST ${NewFILELIST}

if( ${XRootCopyCount} != 0 ) then
        set XRootCopySuccessPercent=`echo "${XRootCopySuccessCount} * 100 / ${XRootCopyCount}" | bc`
        echo "Copied ${XRootCopyCount} files from Xrootd, ${XRootCopySuccessCount} succeeded (${XRootCopySuccessPercent} %)"
endif


echo +++++++++++local copy debug++++++++++++++++++++
echo newFileList: ${FILELIST}
echo ++++++++++++++++cat FILELIST+++++++++++++++++++
cat ${FILELIST}
echo ++++++++++++++++++++find.++++++++++++++++++++++
find .
echo ++++++++++++++++INPUTFILE Var++++++++++++++++++
printenv | grep INPUTFILE
echo +++++++++++++++++++++++++++++++++++++++++++++++

#----------------- End of input copy ----------------------



echo "--------------------------------"
echo 'Simple Unified Meta Scheduler 1.10.50 we are starting in $SCRATCH :' `/bin/pwd`
echo "--------------------------------"
/bin/ls -l

###################################################
# User command BEGIN ----------------------------->

    echo "Star-submit test OK at `date`" > $SCRATCH/test_result_$JOBID.txt && echo "Done"
  
# <------------------------------User command END
###################################################

#Sandbox cleanup script, so user will not copy back sandbox files by using *.*
foreach SANDBOXFILE (`/bin/cat ${SCRATCH}/.sandboxFiles`)
   echo "Cleaning up sandbox file:  ${SCRATCH}/${SANDBOXFILE}"
   /bin/rm -fr ${SCRATCH}/${SANDBOXFILE}
end


# Copy output files (if any where specified)
    
    ########### Copy Command Block ########### 
         
    rm -f $SCRATCH/count.$JOBID.test            
    (/bin/ls $SCRATCH/test_result_$JOBID.txt > $SCRATCH/count.$JOBID.test) >& /dev/null
    if ( -z $SCRATCH/count.$JOBID.test ) then               
        echo 'Error: Scheduler could not find any files in $SCRATCH matching your <output> tag fromScratch value: test_result_$JOBID.txt '
    else                      
        echo "Starting copy block"
        foreach file ( $SCRATCH/test_result_$JOBID.txt )
             set i=0

             if ( -d $file ) then
                set cpCommand="/bin/cp -r $file /star/u/oura/work/star-analysis/job/run/test_star_submit/result/"
             else
                set cpCommand="/bin/cp -fp $file /star/u/oura/work/star-analysis/job/run/test_star_submit/result/"
             endif

             RETRY_CP:
             echo "Running copy command: " $cpCommand
             $cpCommand
             if ( $status && $i < 15 ) then
                 echo "The copy failed, waiting to try again."
                 @ i++
                 sleep 60
                 echo "Trying copy again at "`/bin/date`
                 goto RETRY_CP
             endif
        end
        
    endif
    ##########################################
            
    
    ########### Copy Command Block ########### 
         
    rm -f $SCRATCH/count.$JOBID.test            
    (/bin/ls $SCRATCH/test_result_$JOBID.txt > $SCRATCH/count.$JOBID.test) >& /dev/null
    if ( -z $SCRATCH/count.$JOBID.test ) then               
        echo 'Error: Scheduler could not find any files in $SCRATCH matching your <output> tag fromScratch value: test_result_$JOBID.txt '
    else                      
        echo "Starting copy block"
        foreach file ( $SCRATCH/test_result_$JOBID.txt )
             set i=0

             if ( -d $file ) then
                set cpCommand="/bin/cp -r $file /gpfs01/star/oura/star_submit_test/"
             else
                set cpCommand="/bin/cp -fp $file /gpfs01/star/oura/star_submit_test/"
             endif

             RETRY_CP:
             echo "Running copy command: " $cpCommand
             $cpCommand
             if ( $status && $i < 15 ) then
                 echo "The copy failed, waiting to try again."
                 @ i++
                 sleep 60
                 echo "Trying copy again at "`/bin/date`
                 goto RETRY_CP
             endif
        end
        
    endif
    ##########################################
            
