#!/bin/ash
#
# Script to generate an HTML index of all C code from the current directory
# downwards (skipping directories ending in ~). The header comment in each
# file is listed, and each function's prototype and comment are given. A
# list of "TODO:" comments is also generated.
#
# Outputs the HTML on standard output.
#
# If a parameter is given, it is the prefix to put before any "view file"
# links, eg ".." to link to "../dir/file.c" instead of "dir/file.c".
#
# Skips any files containing the string !NOINDEX.
#
# Requires ctags and cproto.
#

OFFS=$1


# Convert the given string to HTML-escaped values (<, >, & escaped) on
# stdout.
#
html_safe () {
	echo "$*" \
	| sed -e 's|&|\&amp;|g;s|<|\&lt;|g;s|>|\&gt;|g'
}


# Convert the given string to HTML-escaped values (<, >, & escaped) on
# stdout, also adding a <BR> to the end of each line.
#
html_safebr () {
	echo "$*" \
	| sed -e 's|&|\&amp;|g;s|<|\&lt;|g;s|>|\&gt;|g;s/$/<BR>/'
}

ALLFILES=`find . -name '*~' -prune -o -type f -name '*.c' \
          -exec grep -FL '!NOINDEX' /dev/null '{}' ';'`

CTAGDATA=`echo "$ALLFILES" \
          | ctags -nRf- -L- --c-types=f \
          | sed 's/	.\//	/;s/;"	.*$//'`

FILELIST=`echo "$CTAGDATA" | cut -d '	' -f 2 | sort | uniq`

echo '<HTML><HEAD>'
echo '<TITLE>Source Code Index</TITLE>'
echo '</HEAD><BODY>'
echo '<H1><A NAME="top">Source Code Index</A></H1>'
echo '<P><UL>'
echo '<LI><A HREF="#files">File Listing</A></LI>'
echo '<LI><A HREF="#funcs">Function Listing</A></LI>'
echo '<LI><A HREF="#todo">To-Do Listing</A></LI>'
echo '</UL></P>'

echo '<H2><A NAME="files">File Listing</A></H2>'
echo '<P><UL>'
echo "$FILELIST" \
| sed -e \
  's|^.*$|<LI><CODE CLASS="filename"><A HREF="#file-\0">\0</A></CODE></LI>|'
echo '</UL></P>'

for FILE in $FILELIST; do

	DIR=`dirname $FILE`
	FUNCDEFS=`cproto -f1 -I. -Isrc/include -I$DIR $FILE 2>/dev/null \
		  | sed -n 's/^.*[ *]\([^ *(]*\)(.*$/\1/p'`
	FILEHEAD="`sed -n -e \
	          '1,/\*\//{/\/\*/,/\*\//{s/^[\/ *]//;s/^\*[\/]*//;p;};}' \
	          < $FILE`"
	FILESHORTDESC=`echo "$FILEHEAD" | sed -n '1,/^ *$/{/^ *[^ ]*/p;}'`
	FILELONGDESC=`echo "$FILEHEAD" | sed '1,/^ *$/d'`

	echo '<P><HR WIDTH="100%"></P>'
	echo '<P><TABLE BORDER="0"><TR>'
	echo '<TD VALIGN="TOP"><CODE CLASS="filename">'
	echo '<A NAME="file-'"$FILE"'">'"$FILE"'</A></CODE></TD>'
	echo '<TD VALIGN="TOP"> - </TD>'
	echo '<TD VALIGN="TOP">'`html_safe "$FILESHORTDESC"`'</TD>'
	echo '</TR></TABLE></P>'
	echo '<P><SMALL>[<A HREF="'"$OFFS/$FILE"'">View File</A>]</SMALL></P>'
	echo '<P><BLOCKQUOTE>'
	echo "`html_safebr "$FILELONGDESC"`"
	echo '</BLOCKQUOTE></P>'

	if [ -n "$FUNCDEFS" ]; then
		echo '<P>Functions defined:</P>'
		echo '<P><UL>'
		echo "$FUNCDEFS" \
		| sed 's|^.*$|<A HREF="#func-\0---'"$FILE"'">\0</A>|' \
		| sed 's/^/<LI><CODE CLASS="funcname">/;s|$|</CODE></LI>|'
		echo '</UL></P>'
	fi

	echo '<P ALIGN="RIGHT"><SMALL CLASS="navbar">['
	echo '<A HREF="#top">Top</A> |'
	echo '<A HREF="#todo">To Do</A> |'
	echo '<A HREF="#funcs">Functions</A> ]</SMALL></P>'
done

echo '<H2><A NAME="funcs">Function Listing</A></H2>'
echo '<P><UL>'
echo "$CTAGDATA" | while read FUNC FILE LINENUM REST; do
	echo -n '<LI><CODE CLASS="funcname">'
	echo -n '<A HREF="#func-'"$FUNC"'---'"$FILE"'">'"$FUNC"'</A></CODE> '
	echo '[<CODE CLASS="filename">'"$FILE"'</CODE>]</LI>'
done
echo '</UL></P>'

echo "$CTAGDATA" | while read FUNC FILE LINENUM REST; do

	FUNCDEF=`sed -n "$LINENUM,/{/p" < $FILE \
	         | tr '\n' ' ' \
	         | tr -d '{'`

	LASTCOMLINE=`sed -n '1,'"$LINENUM"'{/\/\*/=;}' < $FILE | sed -n '$p'`
	[ -z "$LASTCOMLINE" ] && LASTCOMLINE=1
	LASTENDFUNCLINE=`sed -n '1,'"$LINENUM"'{/}/=;}' < $FILE | sed -n '$p'`
	[ -z "$LASTENDFUNCLINE" ] && LASTENDFUNCLINE=1
	FUNCHEAD="`sed -n -e \
	  "$LASTCOMLINE,"'/\*\//{h;s/^[\/ *]//;s/^\*[\/]*//;p;x;/\*\//q;}' \
	          < $FILE`"
	[ "$LASTCOMLINE" -le "$LASTENDFUNCLINE" ] && FUNCHEAD=""

	echo '<P><HR WIDTH="100%"></P>'
	echo '<P ALIGN="LEFT">'
	echo -n '<CODE CLASS="funcname"><A NAME="func-'"$FUNC"'---'"$FILE"'">'
	echo -n "$FUNC"'</A></CODE> '
	echo -n '[<CODE CLASS="filename"><A HREF="#file-'"$FILE"'">'
	echo "$FILE"'</A></CODE>]'
	echo '</P>'

	echo '<P><CODE CLASS="funcdef">'"`html_safe "$FUNCDEF"`"'</CODE></P>'

	echo '<P><BLOCKQUOTE>'
	echo "`html_safebr "$FUNCHEAD"`"
	echo '</BLOCKQUOTE></P>'

	echo '<P ALIGN="RIGHT"><SMALL CLASS="navbar">['
	echo '<A HREF="#top">Top</A> |'
	echo '<A HREF="#todo">To Do</A> |'
	echo '<A HREF="#files">Files</A> ]</SMALL></P>'
done

echo '<H2><A NAME="todo">To Do Listing</A></H2>'
echo '<P><UL>'
for FILE in $FILELIST; do

	TODOLINES=`sed -n \
	               -e '/\/\*.*\*\//!{/\/\*/,/\*\//{/TODO:/{=;};};}' \
	               -e '/\/\*.*\*\//{/TODO:/{=;};}' \
	           < $FILE`

	[ -z "$TODOLINES" ] && continue

	echo -n '<LI><CODE CLASS="filename">'
	echo '<A HREF="#file-'"$FILE"'">'"$FILE"'</A></CODE>'
	echo '<UL>'

	for NUM in $TODOLINES; do
		TODO=`sed -n "$NUM"'{s/^.*TODO://;s/\*\/.*$//;p;}' < $FILE`
		echo "<LI>[<B>$NUM</B>] `html_safe "$TODO"`</LI>"
	done

	echo '</UL></LI>'
done

echo '</BODY></HTML>'

# EOF
