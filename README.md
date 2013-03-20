# Pipe Viewer - 1.4.6
####Current version: 1.4.6

Pipe Viewer - is a terminal-based tool for monitoring the progress of data through a pipeline. It can be inserted into any normal pipeline between two processes to give a visual indication of how quickly data is passing through, how long it has taken, how near to completion it is, and an estimate of how long it will be until completion.

Additional support is available for multiple instances working in tandem, to given a visual indicator of relative throughput in a complex pipeline: 

![Screen](http://www.ivarch.com/programs/images/pv.png)

####Packages and ports

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

####Install
	./configure
	make
	make install

####Uninstall
	make uninstall

####News

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
