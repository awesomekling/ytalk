#!/bin/sh -x
#
# A compilation of "borrowed" materials.

: ${ACLOCAL="aclocal"}
: ${AUTOMAKE="automake"}
: ${AUTOHEADER="autoheader"}
: ${AUTOCONF="autoconf"}

termcols=`stty -a | sed -n '/columns/s/.*columns \([0-9]*\).*/\1/p'`
status_offset=`expr $termcols - 5`

function smile {
	echo -n "[A[$status_offset"
	echo "G[ [32;01m:)[0m ]"
}

function frown {
	echo -n "[A[$status_offset"
	echo "G[ [31;01m:([0m ]"
	echo "ERROR: $1"
	exit 1
}

echo
echo "* Autogenerating files for YTalk-4.0.0-cvs *"
echo

echo "Cleaning up old files"
rm -rf aclocal.m4 autom4te.cache configure config.h.in Makefile.in \
	&& smile \
	|| frown "Couldn't clean up."

echo "Setting up gettext..."
autopoint -f >/dev/null \
	&& smile \
	|| frown "You need GNU gettext to compile YTalk."

echo "Touching ChangeLog..."
touch ChangeLog
test -f "ChangeLog" \
	&& smile \
	|| frown "Failed to touch ChangeLog"

echo "Running $ACLOCAL..."
WANT_ACLOCAL="1.8" $ACLOCAL -I m4 2>/dev/null || exit 1
test -f "aclocal.m4" \
	&& smile \
	|| frown "Failed to generate aclocal.m4"

echo "Running $AUTOHEADER..."
WANT_AUTOHEADER="2.59" $AUTOHEADER || exit 1
test -f "config.h.in" \
	&& smile \
	|| frown "Failed to generate config.h.in"

echo "Running $AUTOMAKE..."
WANT_AUTOMAKE="1.8" $AUTOMAKE --add-missing --copy --ignore-deps || exit 1
test -f "Makefile.in" \
	&& smile \
	|| frown "Failed to generate Makefile.in"

echo "Running $AUTOCONF..."
WANT_AUTOCONF="2.59" $AUTOCONF || exit 1
test -f configure \
	&& smile \
	frown "Failed to generate configure"

echo "Your stuff is ready."
