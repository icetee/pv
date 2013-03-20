#!/bin/sh
#
# Generate Makefile dependencies inclusion and module target file "depend.mk~"
# by scanning the directory "src" for files ending in ".c" and ".d", and for
# subdirectories not starting with "_".
#
# Modules live inside subdirectories called [^_]* - i.e. a directory "foo" will
# have a rule created which links all code inside it to "foo.o".
#
# The directory "src/include" is never scanned; neither are CVS or SVN
# directories.
#

outlist=$1
outlink=$2

FIND=find
GREP=grep
which gfind 2>/dev/null | grep /gfind >/dev/null && FIND=gfind
which ggrep 2>/dev/null | grep /ggrep >/dev/null && GREP=ggrep

echo '# Automatically generated file listings' > $outlist
echo '#' >> $outlist
echo "# Creation time: `date`" >> $outlist
echo >> $outlist

echo '# Automatically generated module linking rules' > $outlink
echo '#' >> $outlink
echo "# Creation time: `date`" >> $outlink
echo >> $outlink

echo -n "Scanning for source files: "

allsrc=`$FIND src -type f -name "*.c" -print`
allobj=`echo $allsrc | tr ' ' '\n' | sed 's/\.c$/.o/'`
alldep=`echo $allsrc | tr ' ' '\n' | sed 's/\.c$/.d/'`

echo `echo $allsrc | wc -w | tr -d ' '` found

echo -n "Scanning for modules: "

modules=`$FIND src -type d -print \
         | $GREP -v '^src$' \
         | $GREP -v '/_' \
         | $GREP -v '^src/include' \
         | $GREP -v 'CVS' \
         | $GREP -v '.svn' \
         | while read DIR; do \
           CONTENT=\$(/bin/ls -d \$DIR/* \
                     | $GREP -v '.po$' \
                     | $GREP -v '.gmo$' \
                     | $GREP -v '.mo$' \
                     | $GREP -v '.h$' \
                     | sed -n '$p'); \
           [ -n "\$CONTENT" ] || continue; \
           echo \$DIR; \
	   done
         `

echo up to `echo $modules | wc -w | tr -d ' '` found

echo "Writing module linking rules"

echo -n [
for i in $modules; do echo -n ' '; done
echo -n -e ']\r['

for i in $modules; do
  echo -n '.'
  allobj="$allobj $i.o"
  deps=""
  for j in $i/*.c; do
    [ -f $j ] || continue
    newobj=`echo $j | sed -e 's@\.c$@.o@'`
    deps="$deps $newobj"
  done
  for j in $i/*; do
    [ -d "$j" ] || continue
    [ `basename $j` = "CVS" ] && continue
    [ `basename $j` = ".svn" ] && continue
    CONTENT=`/bin/ls -d $j/* \
             | $GREP -v '.po$' \
             | $GREP -v '.gmo$' \
             | $GREP -v '.mo$' \
             | $GREP -v '.h$' \
             | sed -n '$p'`
    [ -n "$CONTENT" ] || continue
    deps="$deps $j.o"
  done
  [ -n "$deps" ] || continue
  echo "$i.o: $deps" >> $outlink
  echo '	$(LD) $(LDFLAGS) -o $@' "$deps" >> $outlink
  echo >> $outlink
done

echo ']'

echo "Listing source, object and dependency files"

echo -n "allsrc = " >> $outlist
echo $allsrc | sed 's,src/nls/cat-id-tbl.c,,' | sed -e 's/ / \\!/g'\
| tr '!' '\n' >> $outlist
echo >> $outlist
echo -n "allobj = " >> $outlist
echo $allobj | sed -e 's/ / \\!/g' | tr '!' '\n' >> $outlist
echo >> $outlist
echo -n "alldep = " >> $outlist
echo $alldep | sed -e 's/ / \\!/g' | tr '!' '\n' >> $outlist

echo >> $outlist
echo >> $outlink

# EOF
