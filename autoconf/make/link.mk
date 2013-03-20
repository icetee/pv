#
# Targets.
#

$(package): src/main.o src/library.o src/pv.o @NLSOBJ@
	$(CC) $(LINKFLAGS) $(CFLAGS) -o $@ src/main.o src/library.o src/pv.o @NLSOBJ@ $(LIBS)

$(package)-static: src/main.o src/library.o src/pv.o @NLSOBJ@
	$(CC) $(LINKFLAGS) $(CFLAGS) -static -o $@ src/main.o src/library.o src/pv.o @NLSOBJ@ $(LIBS)

# EOF
