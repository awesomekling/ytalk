#!/bin/sh

echo "Starting autobuild on $(hostname)" > out.put
uname -a >> out.put
cvs update 2>/dev/null > /dev/null
make distclean 2>/dev/null > /dev/null
echo "--- Running autogen.sh ---" >> out.put
./autogen.sh 2>>out.put >> out.put
echo "--- autogen.sh completed with return code $? ---" >> out.put
echo "--- Running configure ---" >> out.put
CFLAGS="-Wall -pedantic -Wunused -Wimplicit -Wshadow -Wformat" ./configure 2>>out.put >> out.put
echo "--- configure completed with return code $? ---" >> out.put
echo "--- Running make ---" >> out.put
make 2>>out.put >> out.put
echo "--- make completed with return code $? ---" >> out.put
echo "Autobuild completed" >> out.put
