#!/bin/sh
#
# A compilation of "borrowed" materials.

: ${AUTOCONF=autoconf}
: ${AUTOHEADER=autoheader}

echo "Cleaning up..."
rm -rf autom4te.cache configure config.h.in 

echo "Running $AUTOHEADER..."
WANT_AUTOHEADER="2.59" $AUTOHEADER || exit 1
test -f config.h.in || \
	{ echo "FATAL: Failed to generate config.h.in" 2>&1; exit 1; }

echo "Running $AUTOCONF..."
WANT_AUTOCONF="2.59" $AUTOCONF || exit 1
test -f configure || \
	{ echo "FATAL: Failed to generate configure" 2>&1; exit 1; }

echo "Your stuff is ready."
