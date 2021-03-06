#!/bin/sh

: ${ACLOCAL="aclocal"}
: ${AUTOMAKE="automake"}
: ${AUTOHEADER="autoheader"}
: ${AUTOCONF="autoconf"}

tty > /dev/null 2>&1
isatty="$?"

if [ $isatty -eq 0 ] ; then
	if [ -x /usr/ucb/stty ] ; then
		STTY="/usr/ucb/stty"
	else
		STTY="stty"
	fi
	termcols=`$STTY size 2>/dev/null | cut -d' ' -f2`
	status_offset=`expr $termcols - 5`
fi

smile() {
	if [ $isatty -eq 0 ] ; then
		echo -n "[A[$status_offset"
		echo "G[ [32;01m:)[0m ]"
	fi
}

frown() {
	if [ $isatty -eq 0 ] ; then
		echo -n "[A[$status_offset"
		echo "G[ [31;01m:([0m ]"
		echo "ERROR: $1"
		exit 1
	fi
}

label() {
	if [ $isatty -eq 0 ] ; then
		echo " [34;01m*[0m $1"
	else
		echo "* $1"
	fi
}

echo
if [ $isatty -eq 0 ] ; then
	echo "[01mAutogenerating files for YTalk[0m"
else
	echo "Autogenerating files for YTalk"
fi
echo

label "Cleaning up old files"
rm -rf aclocal.m4 autom4te.cache configure config.h.in Makefile.in tmp* \
	&& smile \
	|| frown "Couldn't clean up."

label "Touching ChangeLog..."
touch ChangeLog
test -f "ChangeLog" \
	&& smile \
	|| frown "Failed to touch ChangeLog"

label "Running $ACLOCAL..."
WANT_ACLOCAL="1.9" $ACLOCAL -I m4 2>/dev/null \
	|| frown "Failed to run $ACLOCAL"
test -f "aclocal.m4" \
	&& smile \
	|| frown "Failed to generate aclocal.m4"

label "Running $AUTOHEADER..."
WANT_AUTOHEADER="2.59" $AUTOHEADER \
	|| frown "Failed to run $AUTOHEADER"
test -f "config.h.in" \
	&& smile \
	|| frown "Failed to generate config.h.in"

label "Running $AUTOMAKE..."
WANT_AUTOMAKE="1.9" $AUTOMAKE --add-missing --copy --ignore-deps \
	|| frown "Failed to run $AUTOMAKE"
test -f "Makefile.in" \
	&& smile \
	|| frown "Failed to generate Makefile.in"

label "Running $AUTOCONF..."
WANT_AUTOCONF="2.59" $AUTOCONF \
	|| frown "Failed to run $AUTOCONF"
test -f configure \
	&& smile \
	frown "Failed to generate configure"

label "Cleaning up temp files"
rm -rf tmp* autom4te.cache \
	&& smile \
	|| frown "Couldn't clean up."

echo
echo "Ready to build ( ./configure ; make ; make install )"
echo
