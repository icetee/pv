# Pipe Viewer - 1.6.6
#### Current version: 1.6.6

Pipe Viewer - is a terminal-based tool for monitoring the progress of data through a pipeline. It can be inserted into any normal pipeline between two processes to give a visual indication of how quickly data is passing through, how long it has taken, how near to completion it is, and an estimate of how long it will be until completion.

Additional support is available for multiple instances working in tandem, to given a visual indicator of relative throughput in a complex pipeline:

![Screen](http://www.ivarch.com/programs/images/pv.png)

#### Packages and ports

* CentOS / RHEL:  Set up [YUM repository](http://www.ivarch.com/programs/yum.shtml) or use [RepoForge](http://repoforge.org/), then do **"yum install pv"**.
* Fedora:	Run **"yum install pv"** (the "extras" repository may be required).
* Debian / Ubuntu:	Run **"apt-get install pv"** to get the latest packaged version from "unstable" / "testing". ([Debian QA page here.](http://packages.qa.debian.org/p/pv.html))
* Slackware:	Use this SlackBuild script.
* Cygwin:	Available [as a package](http://slackbuilds.org/repository/13.1/system/pv/). (Christian Franke)
* FreeBSD:	[Listed on FreshPorts](http://www.freshports.org/sysutils/pv).
* OpenBSD:	[Listed under ports](http://www.openbsd.org/cgi-bin/cvsweb/ports/sysutils/pv/).
* Solaris:	Download binary packages from [sunfreeware.com](http://www.sunfreeware.com/introduction.html) or from [OpenCSW](http://www.opencsw.org/packages/pv/).
* OpenSolaris:	Includes pv version 1.1.4 in the "/dev" repository (as of build 119). Install with **"pkg install SUNWpipe-viewer"**. (Menno Lageman)
* AIX:	An RPM from Pawel Piatek is in the downloads section.
* MacPorts:	Run **"port install pv"** to get the latest version. (Brandon Crawford)
* Mac HomeBrew:	Run **"brew install pv"** to get the latest version. (Justin Campbell)
* Exherbo:	Run **"cave resolve -x app-misc/pv"** to get the latest version. (Wulf C. Krueger)
* Gentoo:	Run **"emerge sys-apps/pv"** to get the latest version. (S. Lockwood-Childs; update from Peter Broadwell)
* IRIX Nekoware:	Available in the Nekoware "beta" repository. ([Mark Round](http://www.markround.com/))
* Syabas PopcornHour:	Someone posted t[his port in a forum](http://www.networkedmediatank.com/showthread.php?tid=7192&page=5).

#### Install
	./configure
	make
	make install

#### Documentation

A manual page is included in this distribution.  See 'man ./doc/quickref.1',
or 'man pv' after installation.

#### Uninstall
	make uninstall

#### Author and acknowledgements

This package is copyright 2015 Andrew Wood, and is being distributed under
the terms of the Artistic License 2.0.  For more details of this license,
see the file `doc/COPYING'.

Report bugs in `pv' to pv@ivarch.com or use the contact form linked from the home page.

The `pv' home page is at:

  http://www.ivarch.com/programs/pv.shtml

The latest version can always be found here.

Credit is also due to:

Jakub Hrozek <jhrozek@redhat.com>
- Fedora package maintainer

Antoine Beaupré <anarcat@debian.org>
- Debian package maintainer

Kevin Coyner <kcoyner@debian.org>
Cédric Delfosse <cedric@debian.org>
- previous Debian package maintainers

Eduardo Aguiar <eduardo.oliveira@sondabrasil.com.br>
- provided Portuguese (Brazilian) translation

Stéphane Lacasse <stephane@gorfou.ca>
- provided French translation

Marcos Kreinacke <public@kreinacke.com>
- provided German translation

Bartosz Fenski <fenio@o2.pl> <http://skawina.eu.org/>
- provided Polish translation along with Krystian Zubel

Joshua Jensen
- reported RPM installation bug

Boris Folgmann <http://www.folgmann.com/en/>
- reported cursor handling bug

Mathias Gumz
- reported NLS bug

Daniel Roethlisberger
- submitted patch to use lockfiles for -c if terminal locking fails

Adam Buchbinder
- lots of help with a Cygwin port of -c

Mark Tomich <http://metuchen.dyndns.org>
- suggested -B option

Gert Menke
- reported bug when piping to dd with a large input buffer size

Ville Herva <Ville.Herva@iki.fi>
- informative bug report about rate limiting performance

Elias Pipping
- patch to compile properly on Darwin 9; potential NULL deref report

Patrick Collison
- similar patch for OS X

Boris Lohner
- reported problem that "-L" does not complain if given non-numeric value

Sebastian Kayser
- supplied testing for SIGPIPE, demonstrated internationalisation problem

Laszlo Ersek <http://phptest11.atw.hu/>
- reported shared memory leak on SIGINT with -c

Phil Rutschman <http://bandgap.rsnsoft.com/>
- provided a patch for fully restoring terminal state on exit

Henry Precheur <http://henry.precheur.org/>
- reporting and suggestions for --rate-limit bug when rate is under 10

E. Rosten <http://mi.eng.cam.ac.uk/~er258/>
- supplied patch for block buffering in line mode

Kjetil Torgrim Homme
- reported compilation error with default CFLAGS on non-GCC compilers

Alexandre de Verteuil
- reported bug in OS X build and supplied test environment to fix in

Martin Baum
- supplied patch to return nonzero exit status if terminated by signal

Sam Nelson <http://www.siliconfuture.net/>
- supplied patch to fix trailing slash on DESTDIR

Daniel Pape
- reported Cygwin installation problem due to DESTDIR

Philipp Beckers
- ported to the Syabas PopcornHour A-100 series

Henry Gebhard <hsggebhardt@googlemail.com>
- supplied patches to improve SI prefixes and add --average-rate

Vladimir Kokarev, Alexander Leo
- reported that exit status did not reflect file errors

Thomas Rachel
- submitted patches for IEEE1541 (MiB suffixes), 1+e03 bug

Guillaume Marcais
- submitted speedup patch for line mode

Moritz Barsnick
- submitted patch for compile warning in size calculation

Pawel Piatek
- submitted RPM and patches for AIX

Sami Liedes
- submitted patch for --timer and --bytes with --numeric

Steven Willis
- reported problem with "-R" killing non-PV remote processes

Vladimir Pal, Vladimir Ermakov
- submitted patch which led to development of --format option

Peter Samuelson <peter@p12n.org>
- submitted patch to calculate size if stdout is a block device

Miguel Diaz
- much Cygwin help (and packaging), found narrow-terminal bug

Jim Salter <http://ubuntuwiki.net>
- commissioned work on the --skip-errors option

Wouter Pronk
- reported build problem on SCO

Bryan Dongray <http://www.dongrays.com>
- provided patches for test scripts failing on older Red Hats

Zev Weiss <www.bewilderbeest.net>
- provided patch to fix splice() not using stdin

Zing Shishak
- provided patch for --null / -0 (count null terminated lines)

Jacek Wielemborek <http://deetah.jogger.pl/kategorie/english>
- implemented fdwatch in Python, suggested PV port
- reported bug with "-l" and ETA / size

Kim Krecht
- suggested buffer fill status and last bytes output display options

Cristian Ciupitu <http://ciupicri.github.io>, Josh Stone
- pointed out file descriptor leak with helpful suggestions
  (Josh Stone initially noticed the missing close)

Jan Seda
- found issue with splice() and SPLICE_F_NONBLOCK causing slowdown

André Stapf
- pointed out formatting problem e.g. 13GB -> 13.1GB which should be
  shown 13.0GB -> 13.1GB; highlighted on-startup row swapping in -c

Damon Harper <http://www.usrbin.ca/>
- suggested "-D" / "--delay-start" option

Ganaël Laplanche <http://www.martymac.org>
- provided patch for lstat64 on systems that do not support it

Peter Korsgaard <http://www.buildroot.net/>
- provided similar patch for lstat64, specifically for uClibc support

Ralf Ramsauer <https://blog.ramses-pyramidenbau.de/>
- reported bug which dropped transfer rate on terminal resize

Michiel Van Herwegen
- reported and discussed bug with "-l" and ETA / size

Erkki Seppälä <http://www.inside.org/~flux/>
- provided patch implementing "-I"

####News

1.6.6 - 30 June 2017  

	(r161) use %llu instead of %Lu for better compatibility (Eric A. Borisch)
	(r162) (#1532) fix target buffer size (-B) being ignored (AndCycle, Ilya Basin, Antoine Beaupré)
	(r164) cap read/write sizes, and check elapsed time during read/write cycles, to avoid display hangs with large buffers or slow media; also remove select() call from repeated_write function as it slows the transfer down and the wrapping alarm() means it is unnecessary
	(r169) (#1477) use alternate form for transfer counter, such that 13GB is shown as 13.0GB so it's the same width as 13.1GB (André Stapf)
	(r171) cleanup: units corrections in man page, of the form kb -> KiB
	(r175) report error in "-d" if process fd directory is unreadable, or if process disappears before we start the main loop (Jacek Wielemborek)

1.6.0 - 15 March 2015

	fix lstat64 support when unavailable - separate patches supplied by Ganael Laplanche and Peter Korsgaard
	(#1506) new option "-D" / "--delay-start" to only show bar after N seconds (Damon Harper)
	new option "--fineta" / "-I" to show ETA as time of day rather than time remaining - patch supplied by Erkki Seppälä (r147)
	(#1509) change ETA (--eta / -e) so that days are given if the hours remaining are 24 or more (Jacek Wielemborek)
	(#1499) repeat read and write attempts on partial buffer fill/empty to work around post-signal transfer rate drop reported by Ralf Ramsauer
	(#1507) do not try to calculate total size in line mode, due to bug reported by Jacek Wielemborek and Michiel Van Herwegen
	cleanup: removed defunct RATS comments and unnecessary copyright notices
	clean up displayed lines when using --watchfd PID, when PID exits
	output errors on a new line to avoid overwriting transfer bar

1.5.7 - 26 August 2014

	show KiB instead of incorrect kiB (Debian bug #706175)
	(#1284) do not gzip man page, for non-Linux OSes (Bob Friesenhahn)
	work around awk bug in tests/016-numeric-timer in decimal "," locales
	fix "make rpm" and "make srpm", extend "make release" to sign releases

1.5.3 - 4 May 2014

	remove SPLICE_F_NONBLOCK to fix problem with slow splice() (Jan Seda)

1.5.2 - 10 February 2014

    allow --watchfd to look at block devices
    let --watchfd PID:FD work with --size N
    moved contributors out of the manual as the list was too long
    (NB everyone is still listed in the README and always will be)

1.5.1 - 23 January 2014

    new option --watchfd - suggested by Jacek Wielemborek and "fdwatch"
    use non-block flag with splice()
    new display option --buffer-percent, suggested by Kim Krecht
    new display option --last-written, suggested by Kim Krecht
    new transfer option --no-splice
    fix for minor bug which dropped display elements after one empty one
    fix for single fd leak on exit (Cristian Ciupitu)

1.4.12 - 5 August 2013

	new option --null - patch supplied by Zing Shishak
	AIX build fix (add -lc128) - with help from Pawel Piatek
	AIX -c fixes - with help from Pawel Piatek
	SCO build fix (po2table.sh) - reported by Wouter Pronk
	test scripts fix for older distributions - patch from Bryan Dongray
	fix for splice() not using stdin - patch from Zev Weiss

1.4.6 - 22 January 2013

	added patch from Pawel Piatek to omit O_NOFOLLOW in AIX

1.4.5 - 10 January 2013

	updated manual page to show known problem with "-R" on Cygwin

1.4.4 - 11 December 2012

    added debugging, see "pv -h" when configure run with "--enable-debugging"
    rewrote cursor positioning code used when IPC is unavailable (Cygwin)
    fixed cursor positioning cursor read answerback problem (Cygwin/Solaris)
    ixed bug causing crash when progress displayed with too-small terminal

1.4.0 - 6 December 2012

    new option --skip-errors commissioned by Jim Salter
    if stdout is a block device, and we don't know the total size, use the size of that block device as the total (Peter Samuelson)
    new option --stop-at-size to stop after --size bytes
    report correct filename on read errors
    fix use-after-free bug in remote PID cleanup code
    refactored large chunks of code to make it more readable and to replace most static variables with a state structure

1.3.9 - 5 November 2012

    allow --format parameters to be sent with --remote
    configure option --disable-ipc
    added tests for --numeric with --timer and --bytes
    added tests for --remote

1.3.8 - 29 October 2012

    new --pidfile option to save process ID to a file
    integrated patch for --numeric with --timer and --bytes (Sami Liedes)
    removed signalling from --remote to prevent accidental process kills
    new --format option (originally Vladimir Pal / Vladimir Ermakov)

(23 July 2012)

	Christian Franke reports that PV is available in Cygwin.

1.3.4 - 27 June 2012

    new --disable-splice configure script option
    fixed line mode size count with multiple files (Moritz Barsnick)
    fixes for AIX core dumps (Pawel Piatek)

1.3.1 - 9 June 2012

    do not use splice() if the write buffer is not empty (Thomas Rachel)
    added test 15 (pipe transfers), and new test script

1.3.0 - 5 June 2012

    This version had a bug in splice() handling which could in some circumstances corrupt data. Use 1.3.1 or newer.
    added Tiger build patch from Olle Jonsson
    fix 1024-boundary display garble (Debian bug #586763)
    use splice(2) where available (Debian bug #601683)
    added known bugs section of the manual page
    fixed average rate test, 12 (Andrew Macheret)
    use IEEE1541 units (Thomas Rachel)
    bug with rate limit under 10 fixed (Henry Precheur)
    speed up PV line mode (patch: Guillaume Marcais)
    remove LD=ld from vars.mk to fix cross-compilation (paintitgray/PV#1291)

(19 July 2011)

	AIX RPM supplied by Pawel Piatek (see downloads)

1.2.0 - 14 December 2010

    integrated improved SI prefixes and --average-rate (Henry Gebhardt)
    return nonzero if exiting due to SIGTERM (Martin Baum)
    patch from Phil Rutschman to restore terminal properly on exit
    fix i18n especially for --help (Sebastian Kayser)
    refactored pv_display
    we now have a coherent, documented, exit status
    modified pipe test and new cksum test from Sebastian Kayser
    default CFLAGS to just "-O" for non-GCC (Kjetil Torgrim Homme)
    LFS compile fix for OS X 10.4 (Alexandre de Verteuil)
    remove DESTDIR / suffix (Sam Nelson, Daniel Pape)
	fixed potential NULL deref in transfer (Elias Pipping / LLVM/Clang)

[[Show full history](http://www.ivarch.com/programs/pv.shtml?old#news)]
