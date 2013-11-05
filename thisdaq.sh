#!/bin/bash

DAQdir=$(dirname ${BASH_SOURCE[0]} | xargs readlink -e)

. $DAQdir/root/bin/thisroot.sh

uname=`uname -p`
arch=''
if [ "$uname" == "x86_64" ]; then
    arch='64'
fi

export DAQdir
export MIDASSYS=$DAQdir/midas
export MIDAS_HOME=$MIDASSYS
export PATH=$MIDASSYS/linux$arch/bin:$DAQdir/online-display:$PATH
export LD_LIBRARY_PATH=$MIDASSYS/linux$arch/lib:$PWD/compress:$LD_LIBRARY_PATH
