/*
 * Cursor positioning functions.
 *
 * If IPC is available, then a shared memory segment is used to co-ordinate
 * cursor positioning across multiple instances of `pv'. The shared memory
 * segment contains an integer which is the original "y" co-ordinate of the
 * first `pv' process.
 *
 * However, some OSes (FreeBSD and MacOS X so far) don't allow locking of a
 * terminal, so we try to use a lockfile if terminal locking doesn't work,
 * and finally abort if even that is unavailable.
 */

#include "pv-internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <termios.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>

#ifdef HAVE_IPC
#include <sys/ipc.h>
#include <sys/shm.h>
#ifdef HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif
#ifdef HAVE_LIBGEN_H
#include <libgen.h>
#endif
#endif				/* HAVE_IPC */


/*
 * Create a per-euid, per-tty, lockfile in ${TMPDIR:-${TMP:-/tmp}} for the
 * tty on the given file descriptor.
 */
static void pv_crs_open_lockfile(pvstate_t state, int fd)
{
	char *ttydev;
	char *tmpdir;
	int openflags;

	state->crs_lock_fd = -1;

	ttydev = ttyname(fd);
	if (!ttydev) {
		if (!state->force) {
			pv_error(state, "%s: %s",
				 _("failed to get terminal name"),
				 strerror(errno));
		}
		/*
		 * If we don't know our terminal name, we can neither do IPC
		 * nor make a lock file, so turn off cursor positioning.
		 */
		state->cursor = 0;
		debug("%s",
		      "ttyname failed - cursor positioning disabled");
		return;
	}

	tmpdir = (char *) getenv("TMPDIR");
	if (!tmpdir)
		tmpdir = (char *) getenv("TMP");
	if (!tmpdir)
		tmpdir = "/tmp";

#ifdef HAVE_SNPRINTF
	snprintf(state->crs_lock_file, sizeof(state->crs_lock_file) - 1,
		 "%s/pv-%s-%i.lock", tmpdir, basename(ttydev),
		 (int) geteuid());
#else
	sprintf(state->crs_lock_file,
		"%.*s/pv-%8s-%i.lock",
		sizeof(state->crs_lock_file) - 64, tmpdir,
		basename(ttydev), (int) geteuid());
#endif

	/*
	 * Pawel Piatek - not everyone has O_NOFOLLOW, e.g. AIX doesn't
	 */
#ifdef O_NOFOLLOW
	openflags = O_RDWR | O_CREAT | O_NOFOLLOW;
#else
	openflags = O_RDWR | O_CREAT;
#endif

	state->crs_lock_fd = open(state->crs_lock_file, openflags, 0600);
	if (state->crs_lock_fd < 0) {
		pv_error(state, "%s: %s: %s",
			 state->crs_lock_file,
			 _("failed to open lock file"), strerror(errno));
		state->cursor = 0;
		return;
	}
}


/*
 * Lock the terminal on the given file descriptor, falling back to using a
 * lockfile if the terminal itself cannot be locked.
 */
static void pv_crs_lock(pvstate_t state, int fd)
{
	struct flock lock;
	int lock_fd;

	lock_fd = fd;
	if (state->crs_lock_fd >= 0)
		lock_fd = state->crs_lock_fd;

	lock.l_type = F_WRLCK;
	lock.l_whence = SEEK_SET;
	lock.l_start = 0;
	lock.l_len = 1;
	while (fcntl(lock_fd, F_SETLKW, &lock) < 0) {
		if (errno != EINTR) {
			if (state->crs_lock_fd == -2) {
				pv_crs_open_lockfile(state, fd);
				if (state->crs_lock_fd >= 0) {
					lock_fd = state->crs_lock_fd;
				}
			} else {
				pv_error(state, "%s: %s",
					 _("lock attempt failed"),
					 strerror(errno));
				return;
			}
		}
	}

	if (state->crs_lock_fd >= 0) {
		debug("%s: %s", state->crs_lock_file,
		      "terminal lockfile acquired");
	} else {
		debug("%s", "terminal lock acquired");
	}
}


/*
 * Unlock the terminal on the given file descriptor.  If pv_crs_lock used
 * lockfile locking, unlock the lockfile.
 */
static void pv_crs_unlock(pvstate_t state, int fd)
{
	struct flock lock;
	int lock_fd;

	lock_fd = fd;
	if (state->crs_lock_fd >= 0)
		lock_fd = state->crs_lock_fd;

	lock.l_type = F_UNLCK;
	lock.l_whence = SEEK_SET;
	lock.l_start = 0;
	lock.l_len = 1;
	fcntl(lock_fd, F_SETLK, &lock);

	if (state->crs_lock_fd >= 0) {
		debug("%s: %s", state->crs_lock_file,
		      "terminal lockfile released");
	} else {
		debug("%s", "terminal lock released");
	}
}


#ifdef HAVE_IPC
/*
 * Get the current number of processes attached to our shared memory
 * segment, i.e. find out how many `pv' processes in total are running in
 * cursor mode (including us), and store it in pv_crs_pvcount. If this is
 * larger than pv_crs_pvmax, update pv_crs_pvmax.
 */
static void pv_crs_ipccount(pvstate_t state)
{
	struct shmid_ds buf;

	buf.shm_nattch = 0;

	shmctl(state->crs_shmid, IPC_STAT, &buf);
	state->crs_pvcount = buf.shm_nattch;

	if (state->crs_pvcount > state->crs_pvmax)
		state->crs_pvmax = state->crs_pvcount;

	debug("%s: %d", "pvcount", state->crs_pvcount);
}
#endif				/* HAVE_IPC */


/*
 * Get the current cursor Y co-ordinate by sending the ECMA-48 CPR code to
 * the terminal connected to the given file descriptor.
 */
static int pv_crs_get_ypos(int terminalfd)
{
	struct termios tty;
	struct termios old_tty;
	char cpr[32];
	int ypos;
	int r;
#ifdef CURSOR_ANSWERBACK_BYTE_BY_BYTE
	int got;
#endif				/* CURSOR_ANSWERBACK_BYTE_BY_BYTE */

	tcgetattr(terminalfd, &tty);
	tcgetattr(terminalfd, &old_tty);
	tty.c_lflag &= ~(ICANON | ECHO);
	tcsetattr(terminalfd, TCSANOW | TCSAFLUSH, &tty);

	write(terminalfd, "\033[6n", 4);

	memset(cpr, 0, sizeof(cpr));

#ifdef CURSOR_ANSWERBACK_BYTE_BY_BYTE
	/* Read answerback byte by byte - fails on AIX */
	for (got = 0, r = 0; got < sizeof(cpr) - 2; got += r) {
		r = read(terminalfd, cpr + got, 1);
		if (r <= 0) {
			debug("got=%d, r=%d: %s", got, r, strerror(errno));
			break;
		}
		if (cpr[got] == 'R')
			break;
	}

	debug
	    ("read answerback message from fd %d, length %d - buf = %02X %02X %02X %02X %02X %02X",
	     terminalfd, got, cpr[0], cpr[1], cpr[2], cpr[3], cpr[4],
	     cpr[5]);

#else				/* !CURSOR_ANSWERBACK_BYTE_BY_BYTE */
	/* Read answerback in one big lump - may fail on Solaris */
	r = read(terminalfd, cpr, sizeof(cpr));
	if (r <= 0) {
		debug("r=%d: %s", r, strerror(errno));
	} else {
		debug
		    ("read answerback message from fd %d, length %d - buf = %02X %02X %02X %02X %02X %02X",
		     terminalfd, r, cpr[0], cpr[1], cpr[2], cpr[3], cpr[4],
		     cpr[5]);

	}
#endif				/* CURSOR_ANSWERBACK_BYTE_BY_BYTE */

	ypos = pv_getnum_i(cpr + 2);

	tcsetattr(terminalfd, TCSANOW | TCSAFLUSH, &old_tty);

	debug("%s: %d", "ypos", ypos);

	return ypos;
}


#ifdef HAVE_IPC
/*
 * Initialise the IPC data, returning nonzero on error.
 *
 * To do this, we attach to the shared memory segment (creating it if it
 * does not exist). If we are the only process attached to it, then we
 * initialise it with the current cursor position.
 *
 * There is a race condition here: another process could attach before we've
 * had a chance to check, such that no process ends up getting an "attach
 * count" of one, and so no initialisation occurs. So, we lock the terminal
 * with pv_crs_lock() while we are attaching and checking.
 */
static int pv_crs_ipcinit(pvstate_t state, char *ttyfile, int terminalfd)
{
	key_t key;

	/*
	 * Base the key for the shared memory segment on our current tty, so
	 * we don't end up interfering in any way with instances of `pv'
	 * running on another terminal.
	 */
	key = ftok(ttyfile, 'p');
	if (-1 == key) {
		debug("%s: %s\n", "ftok failed", strerror(errno));
		return 1;
	}

	pv_crs_lock(state, terminalfd);
	if (!state->cursor) {
		debug("%s", "early return - cursor has been disabled");
		return 1;
	}

	state->crs_shmid = shmget(key, sizeof(int), 0600 | IPC_CREAT);
	if (state->crs_shmid < 0) {
		debug("%s: %s", "shmget failed", strerror(errno));
		pv_crs_unlock(state, terminalfd);
		return 1;
	}

	state->crs_y_top = shmat(state->crs_shmid, 0, 0);

	pv_crs_ipccount(state);

	/*
	 * If nobody else is attached to the shared memory segment, we're
	 * the first, so we need to initialise the shared memory with our
	 * current Y cursor co-ordinate.
	 */
	if (state->crs_pvcount < 2) {
		state->crs_y_start = pv_crs_get_ypos(terminalfd);
		*(state->crs_y_top) = state->crs_y_start;
		state->crs_y_lastread = state->crs_y_start;
		debug("%s", "we are the first to attach");
	}

	state->crs_y_offset = state->crs_pvcount - 1;
	if (state->crs_y_offset < 0)
		state->crs_y_offset = 0;

	/*
	 * If anyone else had attached to the shared memory segment, we need
	 * to read the top Y co-ordinate from it.
	 */
	if (state->crs_pvcount > 1) {
		state->crs_y_start = *(state->crs_y_top);
		state->crs_y_lastread = state->crs_y_start;
		debug("%s: %d", "not the first to attach - got top y",
		      state->crs_y_start);
	}

	pv_crs_unlock(state, terminalfd);

	return 0;
}
#endif				/* HAVE_IPC */


/*
 * Initialise the terminal for cursor positioning.
 */
void pv_crs_init(pvstate_t state)
{
	char *ttyfile;
	int fd;

	state->crs_lock_fd = -2;
	state->crs_lock_file[0] = 0;

	if (!state->cursor)
		return;

	debug("%s", "init");

	ttyfile = ttyname(STDERR_FILENO);
	if (!ttyfile) {
		debug("%s: %s",
		      "disabling cursor positioning because ttyname failed",
		      strerror(errno));
		state->cursor = 0;
		return;
	}

	fd = open(ttyfile, O_RDWR);
	if (fd < 0) {
		pv_error(state, "%s: %s: %s",
			 _("failed to open terminal"), ttyfile,
			 strerror(errno));
		state->cursor = 0;
		return;
	}
#ifdef HAVE_IPC
	if (pv_crs_ipcinit(state, ttyfile, fd) != 0) {
		debug("%s", "ipcinit failed, setting noipc flag");
		state->crs_noipc = 1;
	}

	/*
	 * If we are not using IPC, then we need to get the current Y
	 * co-ordinate. If we are using IPC, then the pv_crs_ipcinit()
	 * function takes care of this in a more multi-process-friendly way.
	 */
	if (state->crs_noipc) {
#else				/* ! HAVE_IPC */
	if (1) {
#endif				/* HAVE_IPC */
		/*
		 * Get current cursor position + 1.
		 */
		pv_crs_lock(state, fd);
		state->crs_y_start = pv_crs_get_ypos(fd);
		/*
		 * Move down a line while the terminal is locked, so that
		 * other processes in the pipeline will get a different
		 * initial ypos.
		 */
		if (state->crs_y_start > 0)
			write(STDERR_FILENO, "\n", 1);
		pv_crs_unlock(state, fd);

		if (state->crs_y_start < 1)
			state->cursor = 0;
	}

	close(fd);
}


#ifdef HAVE_IPC
/*
 * Set the "we need to reinitialise cursor positioning" flag.
 */
void pv_crs_needreinit(pvstate_t state)
{
	state->crs_needreinit += 2;
	if (state->crs_needreinit > 3)
		state->crs_needreinit = 3;
}
#endif


#ifdef HAVE_IPC
/*
 * Reinitialise the cursor positioning code (called if we are backgrounded
 * then foregrounded again).
 */
void pv_crs_reinit(pvstate_t state)
{
	debug("%s", "reinit");

	pv_crs_lock(state, STDERR_FILENO);

	state->crs_needreinit--;
	if (state->crs_y_offset < 1)
		state->crs_needreinit = 0;

	if (state->crs_needreinit > 0) {
		pv_crs_unlock(state, STDERR_FILENO);
		return;
	}

	debug("%s", "reinit full");

	state->crs_y_start = pv_crs_get_ypos(STDERR_FILENO);

	if (state->crs_y_offset < 1)
		*(state->crs_y_top) = state->crs_y_start;
	state->crs_y_lastread = state->crs_y_start;

	pv_crs_unlock(state, STDERR_FILENO);
}
#endif


/*
 * Output a single-line update, moving the cursor to the correct position to
 * do so.
 */
void pv_crs_update(pvstate_t state, char *str)
{
	char pos[32];
	int y;

#ifdef HAVE_IPC
	if (!state->crs_noipc) {
		if (state->crs_needreinit)
			pv_crs_reinit(state);

		pv_crs_ipccount(state);
		if (state->crs_y_lastread != *(state->crs_y_top)) {
			state->crs_y_start = *(state->crs_y_top);
			state->crs_y_lastread = state->crs_y_start;
		}

		if (state->crs_needreinit > 0)
			return;
	}
#endif				/* HAVE_IPC */

	y = state->crs_y_start;

#ifdef HAVE_IPC
	/*
	 * If the screen has scrolled, or is about to scroll, due to
	 * multiple `pv' instances taking us near the bottom of the screen,
	 * scroll the screen (only if we're the first `pv'), and then move
	 * our initial Y co-ordinate up.
	 */
	if (((state->crs_y_start + state->crs_pvmax) > state->height)
	    && (!state->crs_noipc)
	    ) {
		int offs;

		offs =
		    ((state->crs_y_start + state->crs_pvmax) -
		     state->height);

		state->crs_y_start -= offs;
		if (state->crs_y_start < 1)
			state->crs_y_start = 1;

		debug("%s: %d", "scroll offset", offs);

		/*
		 * Scroll the screen if we're the first `pv'.
		 */
		if (0 == state->crs_y_offset) {
			pv_crs_lock(state, STDERR_FILENO);

			sprintf(pos, "\033[%d;1H", state->height);
			write(STDERR_FILENO, pos, strlen(pos));
			for (; offs > 0; offs--) {
				write(STDERR_FILENO, "\n", 1);
			}

			pv_crs_unlock(state, STDERR_FILENO);

			debug("%s", "we are the first - scrolled screen");
		}
	}

	if (!state->crs_noipc)
		y = state->crs_y_start + state->crs_y_offset;
#endif				/* HAVE_IPC */

	/*
	 * Keep the Y co-ordinate within sensible bounds, so we can never
	 * overflow the "pos" buffer.
	 */
	if ((y < 1) || (y > 999999))
		y = 1;
	sprintf(pos, "\033[%d;1H", y);

	pv_crs_lock(state, STDERR_FILENO);

	write(STDERR_FILENO, pos, strlen(pos));
	write(STDERR_FILENO, str, strlen(str));

	pv_crs_unlock(state, STDERR_FILENO);
}


/*
 * Reposition the cursor to a final position.
 */
void pv_crs_fini(pvstate_t state)
{
	char pos[32];
	int y;

	debug("%s", "fini");

	y = state->crs_y_start;

#ifdef HAVE_IPC
	if ((state->crs_pvmax > 0) && (!state->crs_noipc))
		y += state->crs_pvmax - 1;
#endif				/* HAVE_IPC */

	if (y > state->height)
		y = state->height;

	/*
	 * Absolute bounds check.
	 */
	if ((y < 1) || (y > 999999))
		y = 1;

	sprintf(pos, "\033[%d;1H\n", y);

	pv_crs_lock(state, STDERR_FILENO);

	write(STDERR_FILENO, pos, strlen(pos));

#ifdef HAVE_IPC
	pv_crs_ipccount(state);
	shmdt((void *) state->crs_y_top);

	/*
	 * If we are the last instance detaching from the shared memory,
	 * delete it so it's not left lying around.
	 */
	if (state->crs_pvcount < 2)
		shmctl(state->crs_shmid, IPC_RMID, 0);

#endif				/* HAVE_IPC */

	pv_crs_unlock(state, STDERR_FILENO);

	if (state->crs_lock_fd >= 0) {
		close(state->crs_lock_fd);
		/*
		 * We can get away with removing this on exit because all
		 * the other PVs will be finishing at the same sort of time.
		 */
		remove(state->crs_lock_file);
	}
}

/* EOF */
