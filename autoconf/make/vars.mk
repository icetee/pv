#
# Variables for Make.
#

srcdir = @srcdir@

prefix = @prefix@
exec_prefix = @exec_prefix@
bindir = @bindir@
infodir = @infodir@
mandir = @mandir@
etcdir = @prefix@/etc
datadir = @datadir@
sbindir = @sbindir@

VPATH = $(srcdir)

localedir = $(datadir)/locale
gnulocaledir = $(prefix)/share/locale

CATALOGS = @CATALOGS@
POFILES = @POFILES@
GMSGFMT = @GMSGFMT@
MSGFMT = @MSGFMT@
XGETTEXT = @XGETTEXT@
MSGMERGE = msgmerge
CATOBJEXT = @CATOBJEXT@
INSTOBJEXT = @INSTOBJEXT@

@SET_MAKE@
SHELL = /bin/sh
CC = @CC@
INSTALL = @INSTALL@
INSTALL_DATA = @INSTALL_DATA@
UNINSTALL = rm -f

LDFLAGS = -r
LINKFLAGS = @LDFLAGS@
DEFS = @DEFS@ -DLOCALEDIR=\"$(localedir)\"
CFLAGS = @CFLAGS@
CPPFLAGS = @CPPFLAGS@ -I$(srcdir)/src/include -Isrc/include $(DEFS)
LIBS = @LIBS@

alltarg = @PACKAGE@

# EOF
