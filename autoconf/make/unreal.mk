#
# Rules for all phony targets.
#

.PHONY: all help make dep depend test check \
  clean depclean indentclean distclean cvsclean svnclean \
  index manhtml indent update-po \
  doc dist release \
  install uninstall \
  rpm srpm

all: $(alltarg) $(CATALOGS)

help:
	@echo 'This Makefile has the following utility targets:'
	@echo
	@echo '  all             build all binary targets'
	@echo '  install         install compiled package and manual'
	@echo '  uninstall       uninstall the package'
	@echo '  check / test    run standardised tests on the compiled binary'
	@echo
	@echo 'Developer targets:'
	@echo
	@echo '  make            rebuild the Makefile (after adding new files)'
	@echo '  dep / depend    rebuild .d (dependency) files'
	@echo '  clean           remove .o (object) and .c~ (backup) files'
	@echo '  depclean        remove .d (dependency) files'
	@echo '  indentclean     remove files left over from "make indent"'
	@echo '  distclean       remove everything not distributed'
	@echo '  cvsclean        remove everything not in CVS/SVN'
	@echo
	@echo '  index           generate an HTML index of source code'
	@echo '  manhtml         output HTML man page to stdout'
	@echo '  indent          reformat all source files with "indent"'
	@echo '  update-po       update the .po files'
	@echo
	@echo '  dist            create a source tarball for distribution'
	@echo '  rpm             build a binary RPM (passes $$RPMFLAGS to RPM)'
	@echo '  srpm            build a source RPM (passes $$RPMFLAGS to RPM)'
	@echo '  release         dist+rpm+srpm'
	@echo

make:
	echo > $(srcdir)/autoconf/make/filelist.mk~
	echo > $(srcdir)/autoconf/make/modules.mk~
	cd $(srcdir); \
	bash autoconf/scripts/makemake.sh \
	     autoconf/make/filelist.mk~ \
	     autoconf/make/modules.mk~
	sh ./config.status

dep depend: $(alldep)
	echo '#' > $(srcdir)/autoconf/make/depend.mk~
	echo '# Dependencies.' >> $(srcdir)/autoconf/make/depend.mk~
	echo '#' >> $(srcdir)/autoconf/make/depend.mk~
	echo >> $(srcdir)/autoconf/make/depend.mk~
	cat $(alldep) >> $(srcdir)/autoconf/make/depend.mk~
	sh ./config.status

clean:
	rm -f $(allobj)

depclean:
	rm -f $(alldep)

indentclean:
	cd $(srcdir) && for FILE in $(allsrc); do rm -f ./$${FILE}~; done

update-po: $(srcdir)/src/nls/$(PACKAGE).pot
	catalogs='$(CATALOGS)'; \
	for cat in $$catalogs; do \
	  lang=$(srcdir)/`echo $$cat | sed 's/$(CATOBJEXT)$$//'`; \
	  mv $$lang.po $$lang.old.po; \
	  if $(MSGMERGE) $$lang.old.po $(srcdir)/src/nls/$(PACKAGE).pot > $$lang.po; then \
	    rm -f $$lang.old.po; \
	  else \
	    echo "msgmerge for $$cat failed!"; \
	    rm -f $$lang.po; \
	    mv $$lang.old.po $$lang.po; \
	    chmod 644 $$lang.po; \
	  fi; \
	done

distclean: clean depclean
	rm -f $(alltarg) src/include/config.h
	rm -rf $(package)-$(version).tar* $(package)-$(version) $(package)-$(version)-*.rpm
	rm -f *.html config.*
	rm Makefile

cvsclean svnclean: distclean
	rm -f doc/lsm
	rm -f doc/$(package).spec
	rm -f doc/quickref.1
	rm -f configure
	rm -f src/nls/*.gmo src/nls/*.mo
	echo > $(srcdir)/autoconf/make/depend.mk~
	echo > $(srcdir)/autoconf/make/filelist.mk~
	echo > $(srcdir)/autoconf/make/modules.mk~

doc:
	:

index:
	(cd $(srcdir); sh autoconf/scripts/index.sh $(srcdir)) > index.html

manhtml:
	@man2html ./doc/quickref.1 \
	| sed -e '1,/<BODY/d' -e '/<\/BODY/,$$d' \
	      -e 's|<A [^>]*>&nbsp;</A>||ig' \
	      -e 's|<A [^>]*>\([^<]*\)</A>|\1|ig' \
	      -e '/<H1/d' -e 's|\(</H[0-9]>\)|\1<P>|ig' \
	      -e 's/<DL COMPACT>/<DL>/ig' \
	      -e 's/&lt;[0-9A-Za-z_.-]\+@[0-9A-Za-z_.-]\+&gt;//g' \
	      -e 's|<I>\(http://.*\)</I>|<A HREF="\1">\1</A>|ig' \
	| sed -e '1,/<HR/d' -e '/<H2>Index/,/<HR/d' \

indent:
	cd $(srcdir) && indent -npro -kr -i8 -cd42 -c45 $(allsrc)

dist: doc update-po
	rm -rf $(package)-$(version)
	mkdir $(package)-$(version)
	cp -dprf Makefile $(distfiles) $(package)-$(version)
	cd $(package)-$(version); $(MAKE) distclean
	cp -dpf doc/lsm             $(package)-$(version)/doc/
	cp -dpf doc/$(package).spec $(package)-$(version)/doc/
	chmod 644 `find $(package)-$(version) -type f -print`
	chmod 755 `find $(package)-$(version) -type d -print`
	chmod 755 `find $(package)-$(version)/autoconf/scripts`
	chmod 755 $(package)-$(version)/configure
	rm -rf DUMMY `find $(package)-$(version) -type d -name CVS`
	rm -rf DUMMY `find $(package)-$(version) -type d -name .svn`
	tar cf $(package)-$(version).tar $(package)-$(version)
	rm -rf $(package)-$(version)
	-gzip -f9 $(package)-$(version).tar

check test: $(alltarg)
	sh $(srcdir)/autoconf/scripts/run-test.sh ./$(package) $(srcdir)

install: all doc
	$(srcdir)/autoconf/scripts/mkinstalldirs \
	  "$(DESTDIR)$(bindir)"
	$(srcdir)/autoconf/scripts/mkinstalldirs \
	  "$(DESTDIR)$(mandir)/man1"
	$(INSTALL) -m 755 $(package) \
	                  "$(DESTDIR)$(bindir)/$(package)"
	$(INSTALL) -m 644 doc/quickref.1 \
	                  "$(DESTDIR)$(mandir)/man1/$(package).1"
	if test -n "$(CATALOGS)"; then \
	  catalogs='$(CATALOGS)'; \
	  for cat in $$catalogs; do \
	    name=`echo $$cat | sed 's,^.*/,,g'`; \
	    if test "`echo $$name | sed 's/.*\(\..*\)/\1/'`" = ".gmo"; then \
	      destdir=$(gnulocaledir); \
	    else \
	      destdir=$(localedir); \
	    fi; \
	    lang=`echo $$name | sed 's/$(CATOBJEXT)$$//'`; \
	    dir=$(DESTDIR)$$destdir/$$lang/LC_MESSAGES; \
	    $(srcdir)/autoconf/scripts/mkinstalldirs $$dir; \
	    $(INSTALL_DATA) $$cat $$dir/$(PACKAGE)$(INSTOBJEXT); \
	  done; \
	fi

uninstall:
	-$(UNINSTALL) "$(DESTDIR)$(bindir)/$(package)"
	-$(UNINSTALL) "$(DESTDIR)$(mandir)/man1/$(package).1"
	-$(UNINSTALL) "$(DESTDIR)$(mandir)/man1/$(package).1.gz"
	-if test -n "$(CATALOGS)"; then \
	  catalogs='$(CATALOGS)'; \
	  for cat in $$catalogs; do \
	    name=`echo $$cat | sed 's,^.*/,,g'`; \
	    if test "`echo $$name | sed 's/.*\(\..*\)/\1/'`" = ".gmo"; then \
	      destdir=$(gnulocaledir); \
	    else \
	      destdir=$(localedir); \
	    fi; \
	    lang=`echo $$name | sed 's/$(CATOBJEXT)$$//'`; \
	    dir=$(DESTDIR)$$destdir/$$lang/LC_MESSAGES; \
	    $(UNINSTALL) $$dir/$(PACKAGE)$(INSTOBJEXT); \
	  done; \
	fi

rpm:
	test -e $(package)-$(version).tar.gz || $(MAKE) dist
	rpmbuild $(RPMFLAGS) --define="%_topdir `pwd`/rpm" -tb $(package)-$(version).tar.gz
	mv rpm/RPMS/*/$(package)-*.rpm .
	rm -rf rpm

srpm:
	test -e $(package)-$(version).tar.gz || $(MAKE) dist
	rpmbuild $(RPMFLAGS) --define="%_topdir `pwd`/rpm" -ts $(package)-$(version).tar.gz
	mv rpm/SRPMS/*$(package)-*.rpm .
	rm -rf rpm

release: dist rpm srpm
	zcat $(package)-$(version).tar.gz | bzip2 > $(package)-$(version).tar.bz2
	-grep -Fq '%_gpg_name' ~/.rpmmacros 2>/dev/null && rpm --addsign *.rpm
	-gpg --list-secret-keys 2>&1 | grep -Fq 'uid' && gpg -ab *.tar.gz && rename .asc .txt *.tar.gz.asc
	-gpg --list-secret-keys 2>&1 | grep -Fq 'uid' && gpg -ab *.tar.bz2 && rename .asc .txt *.tar.bz2.asc
	chmod 644 $(package)-$(version)*
