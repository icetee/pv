#!/bin/sh
#
# Run a test. Parameters are program name and source directory; if
# additional parameters are given, they are the tests to run, otherwise all
# tests are run.
#

PROG="$1"
SRCDIR="$2"
shift
shift
TESTS="$*"

# Temporary working files
#
TMP1=.tmp1
TMP2=.tmp2
TMP3=.tmp3
TMP4=.tmp4

if which mktemp >/dev/null 2>&1; then
	TMP1=`mktemp`
	TMP2=`mktemp`
	TMP3=`mktemp`
	TMP4=`mktemp`
fi

export PROG TMP1 TMP2 TMP3 TMP4	# variables used by test scripts

FAIL=0

test -n "$TESTS" || TESTS=`ls "$SRCDIR/tests" | sort -n`

for SCRIPT in $TESTS; do
	test -f "$SCRIPT" || SCRIPT="$SRCDIR/tests/$SCRIPT"
	test -f "$SCRIPT" || SCRIPT=`ls "$SRCDIR/tests/$SCRIPT"*`
	test -f "$SCRIPT" || continue

	echo `basename "$SCRIPT"`: "                " | cut -b1-20 | sed 's/-/ - /' | tr "\n" ' '

	STATUS=0
	sh -e "$SCRIPT" || STATUS=1
	test $STATUS -eq 1 && FAIL=1

	test $STATUS -eq 1 && echo "FAILED" || echo "OK"
done

rm -f $TMP1 $TMP2 $TMP3 $TMP4

exit $FAIL

# EOF
