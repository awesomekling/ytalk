#!/bin/sh
#
# A compilation of "borrowed" materials.

: ${ACLOCAL=aclocal}
: ${AUTOMAKE=automake}
: ${AUTOHEADER=autoheader}
: ${AUTOCONF=autoconf}

echo "Cleaning up..."
rm -rf aclocal.m4 autom4te.cache configure config.h.in Makefile.in

echo "Touching ChangeLog..."
touch ChangeLog
test -f ChangeLog || \
	{ echo "FATAL: Failed to create ChangeLog" 2>&1; exit 1; }

echo "Running $ACLOCAL..."
WANT_ACLOCAL="1.8" $ACLOCAL -I m4 2>/dev/null || exit 1
test -f aclocal.m4 || \
	{ echo "FATAL: Failed to generate aclocal.m4" 2>&1; exit 1; }

echo "Running $AUTOHEADER..."
WANT_AUTOHEADER="2.59" $AUTOHEADER || exit 1
test -f config.h.in || \
	{ echo "FATAL: Failed to generate config.h.in" 2>&1; exit 1; }

echo "Running $AUTOMAKE..."
WANT_AUTOMAKE="1.8" $AUTOMAKE --add-missing || exit 1
test -f Makefile.in || \
	{ echo "FATAL: Failed to generate Makefile.in" 2>&1; exit 1; }

echo "Running $AUTOCONF..."
WANT_AUTOCONF="2.59" $AUTOCONF || exit 1
test -f configure || \
	{ echo "FATAL: Failed to generate configure" 2>&1; exit 1; }

echo "Your stuff is ready."
