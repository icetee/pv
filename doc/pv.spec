Summary: Monitor the progress of data through a pipe
Name: pv
Version: 1.6.0
Release: 1%{?dist}
License: Artistic 2.0
Group: Development/Tools
Source: http://www.ivarch.com/programs/sources/pv-1.6.0.tar.gz
Url: http://www.ivarch.com/programs/pv.shtml
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)
BuildRequires: gettext

%description
PV ("Pipe Viewer") is a tool for monitoring the progress of data through a
pipeline.  It can be inserted into any normal pipeline between two processes
to give a visual indication of how quickly data is passing through, how long
it has taken, how near to completion it is, and an estimate of how long it
will be until completion.

%prep
%setup -q

%build
%configure
make %{?_smp_mflags}

%install
[ -n "$RPM_BUILD_ROOT" -a "$RPM_BUILD_ROOT" != / ] && rm -rf "$RPM_BUILD_ROOT"
mkdir -p "$RPM_BUILD_ROOT"%{_bindir}
mkdir -p "$RPM_BUILD_ROOT"%{_mandir}/man1
mkdir -p "$RPM_BUILD_ROOT"/usr/share/locale

make DESTDIR="$RPM_BUILD_ROOT" install
%find_lang %{name}

%check
make test

%clean
[ -n "$RPM_BUILD_ROOT" -a "$RPM_BUILD_ROOT" != / ] && rm -rf "$RPM_BUILD_ROOT"

%files -f %{name}.lang
%defattr(-, root, root)
%{_bindir}/%{name}
%{_mandir}/man1/%{name}.1.gz

%doc README doc/NEWS doc/TODO doc/COPYING

%changelog
* Sun Mar 15 2015 Andrew Wood <andrew.wood@ivarch.com> 1.6.0-1
- fix lstat64 support when unavailable - separate patches supplied by Ganael
  Laplanche and Peter Korsgaard
- (#1506) new option "-D" / "--delay-start" to only show bar after N seconds
  (Damon Harper)
- new option "--fineta" / "-I" to show ETA as time of day rather than time
  remaining - patch supplied by Erkki Seppälä (r147)
- (#1509) change ETA (--eta / -e) so that days are given if the hours
  remaining are 24 or more (Jacek Wielemborek)
- (#1499) repeat read and write attempts on partial buffer fill/empty to
  work around post-signal transfer rate drop reported by Ralf Ramsauer
- (#1507) do not try to calculate total size in line mode, due to bug
  reported by Jacek Wielemborek and Michiel Van Herwegen
- cleanup: removed defunct RATS comments and unnecessary copyright notices
- clean up displayed lines when using --watchfd PID, when PID exits
- output errors on a new line to avoid overwriting transfer bar

* Tue Aug 26 2014 Andrew Wood <andrew.wood@ivarch.com> 1.5.7-1
- show KiB instead of incorrect kiB (Debian bug #706175)
- (#1284) do not gzip man page, for non-Linux OSes (Bob Friesenhahn)
- work around "awk" bug in tests/016-numeric-timer in decimal "," locales
- fix "make rpm" and "make srpm", extend "make release" to sign releases

* Sun May  4 2014 Andrew Wood <andrew.wood@ivarch.com> 1.5.3-1
- remove SPLICE_F_NONBLOCK to fix problem with slow splice() (Jan Seda)

* Mon Feb 10 2014 Andrew Wood <andrew.wood@ivarch.com> 1.5.2-1
- allow "--watchfd" to look at block devices
- let "--watchfd PID:FD" work with "--size N"
- moved contributors out of the manual as the list was too long
- (NB everyone is still listed in the README and always will be)

* Thu Jan 23 2014 Andrew Wood <andrew.wood@ivarch.com> 1.5.1-1
- new option "--watchfd" - suggested by Jacek Wielemborek and "fdwatch"
- use non-block flag with splice()
- new display option "--buffer-percent", suggested by Kim Krecht
- new display option "--last-written", suggested by Kim Krecht
- new transfer option "--no-splice"
- fix for minor bug which dropped display elements after one empty one
- fix for single fd leak on exit (Cristian Ciupitu, Josh Stone)

* Mon Aug  5 2013 Andrew Wood <andrew.wood@ivarch.com> 1.4.12-1
- new option "--null" - patch supplied by Zing Shishak
- AIX build fix (add "-lc128") - with help from Pawel Piatek
- AIX "-c" fixes - with help from Pawel Piatek
- SCO build fix (po2table.sh) - reported by Wouter Pronk
- test scripts fix for older distributions - patch from Bryan Dongray
- fix for splice() not using stdin - patch from Zev Weiss

* Tue Jan 22 2013 Andrew Wood <andrew.wood@ivarch.com> 1.4.6-1
- added patch from Pawel Piatek to omit O_NOFOLLOW in AIX

* Thu Jan 10 2013 Andrew Wood <andrew.wood@ivarch.com> 1.4.5-1
- updated manual page to show known problem with "-R" on Cygwin

* Tue Dec 11 2012 Andrew Wood <andrew.wood@ivarch.com> 1.4.4-1
- added debugging, see `pv -h' when configure run with "--enable-debugging"
- rewrote cursor positioning code used when IPC is unavailable (Cygwin)
- fixed cursor positioning cursor read answerback problem (Cygwin/Solaris)
- fixed bug causing crash when progress displayed with too-small terminal

* Thu Dec  6 2012 Andrew Wood <andrew.wood@ivarch.com> 1.4.0-1
- new option "--skip-errors" commissioned by Jim Salter
- if stdout is a block device, and we don't know the total size, use the
  size of that block device as the total (Peter Samuelson)
- new option "--stop-at-size" to stop after "--size" bytes
- report correct filename on read errors
- fix use-after-free bug in remote PID cleanup code
- refactored large chunks of code to make it more readable and to replace
  most static variables with a state structure

* Mon Nov  5 2012 Andrew Wood <andrew.wood@ivarch.com> 1.3.9-1
- allow "--format" parameters to be sent with "--remote"
- configure option "--disable-ipc"
- added tests for --numeric with --timer and --bytes
- added tests for --remote

* Mon Oct 29 2012 Andrew Wood <andrew.wood@ivarch.com> 1.3.8-1
- new "--pidfile" option to save process ID to a file
- integrated patch for --numeric with --timer and --bytes (Sami Liedes)
- removed signalling from --remote to prevent accidental process kills
- new "--format" option (originally Vladimir Pal / Vladimir Ermakov)

* Wed Jun 27 2012 Andrew Wood <andrew.wood@ivarch.com> 1.3.4-1
- new "--disable-splice" configure script option
- fixed line mode size count with multiple files (Moritz Barsnick)
- fixes for AIX core dumps (Pawel Piatek)

* Sat Jun  9 2012 Andrew Wood <andrew.wood@ivarch.com> 1.3.1-1
- do not use splice() if the write buffer is not empty (Thomas Rachel)
- added test 15 (pipe transfers), and new test script

* Tue Jun  5 2012 Andrew Wood <andrew.wood@ivarch.com> 1.3.0-1
- added Tiger build patch from Olle Jonsson.
- fix 1024-boundary display garble (Debian bug #586763).
- use splice(2) where available (Debian bug #601683).
- added known bugs section of the manual page.
- fixed average rate test, 12 (Andrew Macheret).
- use IEEE1541 units (Thomas Rachel).
- bug with rate limit under 10 fixed (Henry Precheur).
- speed up PV line mode (patch: Guillaume Marcais).
- remove LD=ld from vars.mk to fix cross-compilation (paintitgray/PV#1291).

* Tue Dec 14 2010 Andrew Wood <andrew.wood@ivarch.com> 1.2.0-1
- Integrated improved SI prefixes and --average-rate (Henry Gebhardt).
- Return nonzero if exiting due to SIGTERM (Martin Baum).
- Patch from Phil Rutschman to restore terminal properly on exit.
- Fix i18n especially for --help (Sebastian Kayser).
- Refactored pv_display.
- We now have a coherent, documented, exit status.
- Modified pipe test and new cksum test from Sebastian Kayser.
- Default CFLAGS to just "-O" for non-GCC (Kjetil Torgrim Homme).
- LFS compile fix for OS X 10.4 (Alexandre de Verteuil).
- Remove DESTDIR / suffix (Sam Nelson, Daniel Pape).
- Fixed potential NULL deref in transfer (Elias Pipping / LLVM/Clang).

* Thu Mar  6 2008 Andrew Wood <andrew.wood@ivarch.com> 1.1.4-1
- Trap SIGINT/SIGHUP/SIGTERM so we clean up IPCs on exit (Laszlo Ersek).
- Abort if numeric option, eg -L, has non-numeric value (Boris Lohner).
- Compilation fixes for Darwin 9 and OS X.

* Thu Aug 30 2007 Andrew Wood <andrew.wood@ivarch.com> 1.1.0-1
- New option "-R" to remotely control another pv process.
- New option "-l" to count lines instead of bytes.
- Performance improvement for "-L" (rate) option.
- Some Mac OS X fixes, and packaging cleanups.

* Sat Aug  4 2007 Andrew Wood <andrew.wood@ivarch.com> 1.0.1-1
- Changed license from Artistic to Artistic 2.0.
- Removed "--license" option.

* Thu Aug  2 2007 Andrew Wood <andrew.wood@ivarch.com> 1.0.0-1
- We now act more like "cat" - just skip unreadable files, don't abort.
- Various code cleanups were done.

* Mon Feb  5 2007 Andrew Wood <andrew.wood@ivarch.com> 0.9.9-1
- New option "-B" to set the buffer size, and a workaround for problems
- piping to dd(1).

* Mon Feb 27 2006 Andrew Wood <andrew.wood@ivarch.com>
- Minor bugfixes, and on the final update, blank out the now-zero ETA.

* Thu Sep  1 2005 Andrew Wood <andrew.wood@ivarch.com>
- Terminal locking now uses lockfiles if the terminal itself cannot be locked.

* Thu Jun 16 2005 Andrew Wood <andrew.wood@ivarch.com>
- A minor problem with the spec file was fixed.

* Mon Nov 15 2004 Andrew Wood <andrew.wood@ivarch.com>
- A minor bug in the NLS code was fixed.

* Sat Nov  6 2004 Andrew Wood <andrew.wood@ivarch.com>
- Code cleanups and minor usability fixes.

* Tue Jun 29 2004 Andrew Wood <andrew.wood@ivarch.com>
- A port of the terminal locking code to FreeBSD.

* Sun May  2 2004 Andrew Wood <andrew.wood@ivarch.com>
- Major reliability improvements to the cursor positioning.

* Sat Apr 24 2004 Andrew Wood <andrew.wood@ivarch.com>
- Rate and size parameters can now take suffixes such as "k", "m" etc.

* Mon Apr 19 2004 Andrew Wood <andrew.wood@ivarch.com>
- A bug in the cursor positioning was fixed.

* Thu Feb 12 2004 Andrew Wood <andrew.wood@ivarch.com>
- Code cleanups and portability fixes.

* Sun Feb  8 2004 Andrew Wood <andrew.wood@ivarch.com>
- The display buffer is now dynamically allocated, fixing an overflow bug.

* Wed Jan 14 2004 Andrew Wood <andrew.wood@ivarch.com>
- A minor bug triggered when installing the RPM was fixed.

* Mon Dec 22 2003 Andrew Wood <andrew.wood@ivarch.com>
- Fixed a minor bug that occasionally reported "resource unavailable".

* Wed Aug  6 2003 Andrew Wood <andrew.wood@ivarch.com>
- Block devices now have their size read correctly, so pv /dev/hda1 works
- Minor code cleanups (mainly removal of CVS "Id" tags)

* Sun Aug  3 2003 Andrew Wood <andrew.wood@ivarch.com>
- Doing ^Z then "bg" then "fg" now continues displaying

* Tue Jul 16 2002 Andrew Wood <andrew.wood@ivarch.com>
- First draft of spec file created.
