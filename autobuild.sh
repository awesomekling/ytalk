#!/bin/sh

echo "+++ Starting autobuild on $(hostname)"
echo "+++ Date: `date`"
echo "+++ SMRN: `uname -smrn`"
cvs update >/dev/null 2>&1
echo ">>> autogen.sh"
./autogen.sh
echo "--- autogen.sh completed with return code $?"
echo ">>> configure"
CFLAGS="-W -Wall -pedantic" ./configure
echo "--- configure completed with return code $?"
echo ">>> make clean"
make clean
echo ">>> make"
make
echo "--- make completed with return code $?"
echo "+++ Autobuild completed"
