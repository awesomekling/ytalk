#!/bin/sh

echo "+++ Starting autobuild on `hostname | cut -f1 -d.`"
echo "+++ Date: `date`"
echo "+++ SNRM: `uname -snrm`"
cvs update -dP >/dev/null 2>&1
echo ">>> autogen.sh"
./autogen.sh
echo "--- autogen.sh completed with return code $?"
echo ">>> configure"
./configure $AUTOCONF_ARGS
echo "--- configure completed with return code $?"
echo ">>> make clean"
make clean
echo ">>> make"
make
echo "--- make completed with return code $?"
echo ">>> ytalk -v"
src/ytalk -v
echo "--- ytalk -v finished with return code $?"
echo "+++ Autobuild completed at `date`"
