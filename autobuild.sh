#!/bin/sh

echo "+++ Starting autobuild on $(hostname)"
echo "+++ Date: `date`"
echo "+++ SNRM: `uname -smrn`"
cvs update >/dev/null 2>&1
echo ">>> autogen.sh"
./autogen.sh
echo "--- autogen.sh completed with return code $?"
echo ">>> configure"
CFLAGS="$CFLAGS -W -Wall -pedantic" ./configure $AUTOCONF_ARGS
echo "--- configure completed with return code $?"
echo ">>> make clean"
make clean
echo ">>> make"
make
echo "--- make completed with return code $?"
echo "+++ Autobuild completed"
