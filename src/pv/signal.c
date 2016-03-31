/*
 * Signal handling functions.
 */

#include "pv-internal.h"

#include <signal.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>

#ifdef HAVE_IPC
void pv_crs_needreinit(pvstate_t);
#endif

static pvstate_t pv_sig_state = NULL;


/*
 * Handle SIGTTOU (tty output for background process) by redirecting stderr
 * to /dev/null, so that we can be stopped and backgrounded without messing
 * up the terminal. We store the old stderr file descriptor so that on a
 * subsequent SIGCONT we can try writing to the terminal again, in case we
 * get backgrounded and later get foregrounded again.
 */
static void pv_sig_ttou(int s)
{
	int fd;

	fd = open("/dev/null", O_RDWR);
	if (fd < 0)
		return;

	if (-1 == pv_sig_state->pv_sig_old_stderr)
		pv_sig_state->pv_sig_old_stderr = dup(STDERR_FILENO);

	dup2(fd, STDERR_FILENO);
	close(fd);
}


/*
 * Handle SIGTSTP (stop typed at tty) by storing the time the signal
 * happened for later use by pv_sig_cont(), and then stopping the process.
 */
static void pv_sig_tstp(int s)
{
	gettimeofday(&(pv_sig_state->pv_sig_tstp_time), NULL);
	raise(SIGSTOP);
}


/*
 * Handle SIGCONT (continue if stopped) by adding the elapsed time since the
 * last SIGTSTP to the elapsed time offset, and by trying to write to the
 * terminal again (by replacing the /dev/null stderr with the old stderr).
 */
static void pv_sig_cont(int s)
{
	struct timeval tv;
	struct termios t;

	pv_sig_state->pv_sig_newsize = 1;

	if (0 == pv_sig_state->pv_sig_tstp_time.tv_sec) {
		tcgetattr(STDERR_FILENO, &t);
		t.c_lflag |= TOSTOP;
		tcsetattr(STDERR_FILENO, TCSANOW, &t);
#ifdef HAVE_IPC
		pv_crs_needreinit(pv_sig_state);
#endif
		return;
	}

	gettimeofday(&tv, NULL);

	pv_sig_state->pv_sig_toffset.tv_sec +=
	    (tv.tv_sec - pv_sig_state->pv_sig_tstp_time.tv_sec);
	pv_sig_state->pv_sig_toffset.tv_usec +=
	    (tv.tv_usec - pv_sig_state->pv_sig_tstp_time.tv_usec);
	if (pv_sig_state->pv_sig_toffset.tv_usec >= 1000000) {
		pv_sig_state->pv_sig_toffset.tv_sec++;
		pv_sig_state->pv_sig_toffset.tv_usec -= 1000000;
	}
	if (pv_sig_state->pv_sig_toffset.tv_usec < 0) {
		pv_sig_state->pv_sig_toffset.tv_sec--;
		pv_sig_state->pv_sig_toffset.tv_usec += 1000000;
	}

	pv_sig_state->pv_sig_tstp_time.tv_sec = 0;
	pv_sig_state->pv_sig_tstp_time.tv_usec = 0;

	if (pv_sig_state->pv_sig_old_stderr != -1) {
		dup2(pv_sig_state->pv_sig_old_stderr, STDERR_FILENO);
		close(pv_sig_state->pv_sig_old_stderr);
		pv_sig_state->pv_sig_old_stderr = -1;
	}

	tcgetattr(STDERR_FILENO, &t);
	t.c_lflag |= TOSTOP;
	tcsetattr(STDERR_FILENO, TCSANOW, &t);

#ifdef HAVE_IPC
	pv_crs_needreinit(pv_sig_state);
#endif
}


/*
 * Handle SIGWINCH (window size changed) by setting a flag.
 */
static void pv_sig_winch(int s)
{
	pv_sig_state->pv_sig_newsize = 1;
}


/*
 * Handle termination signals by setting the abort flag.
 */
static void pv_sig_term(int s)
{
	pv_sig_state->pv_sig_abort = 1;
}


/*
 * Initialise signal handling.
 */
void pv_sig_init(pvstate_t state)
{
	struct sigaction sa;

	pv_sig_state = state;

	pv_sig_state->pv_sig_old_stderr = -1;
	pv_sig_state->pv_sig_tstp_time.tv_sec = 0;
	pv_sig_state->pv_sig_tstp_time.tv_usec = 0;
	pv_sig_state->pv_sig_toffset.tv_sec = 0;
	pv_sig_state->pv_sig_toffset.tv_usec = 0;

	/*
	 * Ignore SIGPIPE, so we don't die if stdout is a pipe and the other
	 * end closes unexpectedly.
	 */
	sa.sa_handler = SIG_IGN;
	sigemptyset(&(sa.sa_mask));
	sa.sa_flags = 0;
	sigaction(SIGPIPE, &sa, &(pv_sig_state->pv_sig_old_sigpipe));

	/*
	 * Handle SIGTTOU by continuing with output switched off, so that we
	 * can be stopped and backgrounded without messing up the terminal.
	 */
	sa.sa_handler = pv_sig_ttou;
	sigemptyset(&(sa.sa_mask));
	sa.sa_flags = 0;
	sigaction(SIGTTOU, &sa, &(pv_sig_state->pv_sig_old_sigttou));

	/*
	 * Handle SIGTSTP by storing the time the signal happened for later
	 * use by pv_sig_cont(), and then stopping the process.
	 */
	sa.sa_handler = pv_sig_tstp;
	sigemptyset(&(sa.sa_mask));
	sa.sa_flags = 0;
	sigaction(SIGTSTP, &sa, &(pv_sig_state->pv_sig_old_sigtstp));

	/*
	 * Handle SIGCONT by adding the elapsed time since the last SIGTSTP
	 * to the elapsed time offset, and by trying to write to the
	 * terminal again.
	 */
	sa.sa_handler = pv_sig_cont;
	sigemptyset(&(sa.sa_mask));
	sa.sa_flags = 0;
	sigaction(SIGCONT, &sa, &(pv_sig_state->pv_sig_old_sigcont));

	/*
	 * Handle SIGWINCH by setting a flag to let the main loop know it
	 * has to reread the terminal size.
	 */
	sa.sa_handler = pv_sig_winch;
	sigemptyset(&(sa.sa_mask));
	sa.sa_flags = 0;
	sigaction(SIGWINCH, &sa, &(pv_sig_state->pv_sig_old_sigwinch));

	/*
	 * Handle SIGINT, SIGHUP, SIGTERM by setting a flag to let the
	 * main loop know it should quit now.
	 */
	sa.sa_handler = pv_sig_term;
	sigemptyset(&(sa.sa_mask));
	sa.sa_flags = 0;
	sigaction(SIGINT, &sa, &(pv_sig_state->pv_sig_old_sigint));

	sa.sa_handler = pv_sig_term;
	sigemptyset(&(sa.sa_mask));
	sa.sa_flags = 0;
	sigaction(SIGHUP, &sa, &(pv_sig_state->pv_sig_old_sighup));

	sa.sa_handler = pv_sig_term;
	sigemptyset(&(sa.sa_mask));
	sa.sa_flags = 0;
	sigaction(SIGTERM, &sa, &(pv_sig_state->pv_sig_old_sigterm));
}


/*
 * Shut down signal handling.
 */
void pv_sig_fini(pvstate_t state)
{
	sigaction(SIGPIPE, &(pv_sig_state->pv_sig_old_sigpipe), NULL);
	sigaction(SIGTTOU, &(pv_sig_state->pv_sig_old_sigttou), NULL);
	sigaction(SIGTSTP, &(pv_sig_state->pv_sig_old_sigtstp), NULL);
	sigaction(SIGCONT, &(pv_sig_state->pv_sig_old_sigcont), NULL);
	sigaction(SIGWINCH, &(pv_sig_state->pv_sig_old_sigwinch), NULL);
	sigaction(SIGINT, &(pv_sig_state->pv_sig_old_sigint), NULL);
	sigaction(SIGHUP, &(pv_sig_state->pv_sig_old_sighup), NULL);
	sigaction(SIGTERM, &(pv_sig_state->pv_sig_old_sigterm), NULL);
}


/*
 * Stop reacting to SIGTSTP and SIGCONT.
 */
void pv_sig_nopause(void)
{
	struct sigaction sa;

	sa.sa_handler = SIG_IGN;
	sigemptyset(&(sa.sa_mask));
	sa.sa_flags = 0;
	sigaction(SIGTSTP, &sa, NULL);

	sa.sa_handler = SIG_DFL;
	sigemptyset(&(sa.sa_mask));
	sa.sa_flags = 0;
	sigaction(SIGCONT, &sa, NULL);
}


/*
 * Start catching SIGTSTP and SIGCONT again.
 */
void pv_sig_allowpause(void)
{
	struct sigaction sa;

	sa.sa_handler = pv_sig_tstp;
	sigemptyset(&(sa.sa_mask));
	sa.sa_flags = 0;
	sigaction(SIGTSTP, &sa, NULL);

	sa.sa_handler = pv_sig_cont;
	sigemptyset(&(sa.sa_mask));
	sa.sa_flags = 0;
	sigaction(SIGCONT, &sa, NULL);
}


/*
 * If we have redirected stderr to /dev/null, check every second or so to
 * see whether we can write to the terminal again - this is so that if we
 * get backgrounded, then foregrounded again, we start writing to the
 * terminal again.
 */
void pv_sig_checkbg(void)
{
	static time_t next_check = 0;
	struct termios t;

	if (time(NULL) < next_check)
		return;

	next_check = time(NULL) + 1;

	if (-1 == pv_sig_state->pv_sig_old_stderr)
		return;

	dup2(pv_sig_state->pv_sig_old_stderr, STDERR_FILENO);
	close(pv_sig_state->pv_sig_old_stderr);
	pv_sig_state->pv_sig_old_stderr = -1;

	tcgetattr(STDERR_FILENO, &t);
	t.c_lflag |= TOSTOP;
	tcsetattr(STDERR_FILENO, TCSANOW, &t);

#ifdef HAVE_IPC
	pv_crs_needreinit(pv_sig_state);
#endif
}

/* EOF */
