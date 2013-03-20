/*
 * Functions internal to the PV library.
 *
 * Copyright 2013 Andrew Wood, distributed under the Artistic License 2.0.
 */

#ifndef _PV_INTERNAL_H
#define _PV_INTERNAL_H 1

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifndef _PV_H
#include "pv.h"
#endif /* _PV_H */

#include <signal.h>
#include <sys/types.h>
#include <sys/time.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PV_DISPLAY_PROGRESS	1
#define PV_DISPLAY_TIMER	2
#define PV_DISPLAY_ETA		4
#define PV_DISPLAY_RATE		8
#define PV_DISPLAY_AVERAGERATE	16
#define PV_DISPLAY_BYTES	32
#define PV_DISPLAY_NAME		64

#define RATE_GRANULARITY	100000	 /* usec between -L rate chunks */
#define REMOTE_INTERVAL		100000	 /* usec between checks for -R */
#define BUFFER_SIZE		409600	 /* default transfer buffer size */
#define BUFFER_SIZE_MAX		524288	 /* max auto transfer buffer size */

#define MAXIMISE_BUFFER_FILL	1


/*
 * Structure for holding PV internal state. Opaque outside the PV library.
 */
struct pvstate_s {
	/***************
	 * Input files *
	 ***************/
	int input_file_count;		 /* number of input files */
	const char **input_files;	 /* input files (0=first) */

	/*******************
	 * Program control *
	 *******************/
	unsigned char force;             /* display even if not on terminal */
	unsigned char cursor;            /* use cursor positioning */
	unsigned char numeric;           /* numeric output only */
	unsigned char wait;              /* wait for data before display */
	unsigned char linemode;          /* count lines instead of bytes */
	unsigned char no_op;             /* do nothing other than pipe data */
	unsigned char skip_errors;       /* skip read errors flag */
	unsigned char stop_at_size;      /* set if we stop at "size" bytes */
	unsigned long long rate_limit;   /* rate limit, in bytes per second */
	unsigned long long target_buffer_size;  /* buffer size (0=default) */
	unsigned long long size;         /* total size of data */
	double interval;                 /* interval between updates */
	unsigned int width;              /* screen width */
	unsigned int height;             /* screen height */
	const char *name;		 /* display name */
	char default_format[512];	 /* default format string */
	const char *format_string;	 /* output format string */

	/******************
	 * Program status *
	 ******************/
	const char *program_name;	 /* program name for error reporting */
	const char *current_file;	 /* current file being read */
	int exit_status; 		 /* exit status to give (0=OK) */

	/*******************
	 * Signal handling *
	 *******************/
	int pv_sig_old_stderr;		 /* see pv_sig_ttou() */
	struct timeval pv_sig_tstp_time; /* see pv_sig_tstp() / __cont() */
	struct timeval pv_sig_toffset;		 /* total time spent stopped */
	volatile sig_atomic_t pv_sig_newsize;	 /* whether we need to get term size again */
	volatile sig_atomic_t pv_sig_abort;		 /* whether we need to abort right now */
	volatile sig_atomic_t reparse_display;
	struct sigaction pv_sig_old_sigpipe;
	struct sigaction pv_sig_old_sigttou;
	struct sigaction pv_sig_old_sigtstp;
	struct sigaction pv_sig_old_sigcont;
	struct sigaction pv_sig_old_sigwinch;
	struct sigaction pv_sig_old_sigint;
	struct sigaction pv_sig_old_sighup;
	struct sigaction pv_sig_old_sigterm;

	/*****************
	 * Display state *
	 *****************/
	long percentage;
	long double prev_elapsed_sec;
	long double prev_rate;
	long double prev_trans;
	char *display_buffer;
	long display_buffer_size;
	int prev_width;			 /* screen width last time we were called */
	int prev_length;		 /* length of last string we output */
	char str_name[512];		 /* RATS: ignore (big enough) */
	char str_transferred[128];	 /* RATS: ignore (big enough) */
	char str_timer[128];		 /* RATS: ignore (big enough) */
	char str_rate[128];		 /* RATS: ignore (big enough) */
	char str_average_rate[128];	 /* RATS: ignore (big enough) */
	char str_progress[1024];	 /* RATS: ignore (big enough) */
	char str_eta[128];		 /* RATS: ignore (big enough) */
	unsigned long components_used;	 /* bitmask of components used */
	struct {
		const char *string;
		int length;
	} format[100];

	/********************
	 * Cursor/IPC state *
	 ********************/
#ifdef HAVE_IPC
	int crs_shmid;			 /* ID of our shared memory segment */
	int crs_pvcount;		 /* number of `pv' processes in total */
	int crs_pvmax;			 /* highest number of `pv's seen */
	int *crs_y_top;			 /* pointer to Y coord of topmost `pv' */
	int crs_y_lastread;		 /* last value of _y_top seen */
	int crs_y_offset;		 /* our Y offset from this top position */
	int crs_needreinit;		 /* set if we need to reinit cursor pos */
	int crs_noipc;			 /* set if we can't use IPC */
#endif				/* HAVE_IPC */
	int crs_lock_fd;		 /* fd of lockfile, -1 if none open */
	char crs_lock_file[1024];	 /* RATS: ignore */
	int crs_y_start;		 /* our initial Y coordinate */

	/*******************
	 * Transfer state  *
	 *******************/
	/*
	 * The transfer buffer is used for moving data from the input files
	 * to the output when splice() is not available.
	 *
	 * If buffer_size is smaller than pv__target_bufsize, then
	 * pv_transfer will try to reallocate transfer_buffer to make
	 * buffer_size equal to pv__target_bufsize.
	 *
	 * Data from the input files is read into the buffer; read_position
	 * is the offset in the buffer that we've read data up to.
	 *
	 * Data is written to the output from the buffer, and write_position
	 * is the offset in the buffer that we've written data up to.  It
	 * will always be less than or equal to read_position.
	 */
	unsigned char *transfer_buffer;	 /* data transfer buffer */
	unsigned long long buffer_size;	 /* size of buffer */
	unsigned long read_position;	 /* amount of data in buffer */
	unsigned long write_position;	 /* buffered data written */

	/*
	 * While reading from a file descriptor we keep track of how many
	 * times in a row we've seen errors (read_errors_in_a_row), and
	 * whether or not we have put a warning on stderr about read errors
	 * on this fd (read_error_warning_shown).
	 *
	 * Whenever the active file descriptor changes from
	 * last_read_skip_fd, we reset read_errors_in_a_row and
	 * read_error_warning_shown to 0 for the new file descriptor and set
	 * last_read_skip_fd to the new fd number.
	 *
	 * This way, we're treating each input file separately.
	 */
	int last_read_skip_fd;
	unsigned long read_errors_in_a_row;
	int read_error_warning_shown;
#ifdef HAVE_SPLICE
	/*
	 * These variables are used to keep track of whether splice() was
	 * used; splice_failed_fd is the file descriptor that splice() last
	 * failed on, so that we don't keep trying to use it on an fd that
	 * doesn't support it, and splice_used is set to 1 if splice() was
	 * used this time within pv_transfer().
	 */
	int splice_failed_fd;
	int splice_used;
#endif
	long to_write;			 /* max to write this time around */
	long written;			 /* bytes sent to stdout this time */
};


int pv_main_loop(pvstate_t);
void pv_display(pvstate_t, long double, long long, long long);
long pv_transfer(pvstate_t, int, int *, int *, unsigned long long, long *);
void pv_set_buffer_size(unsigned long long, int);
int pv_next_file(pvstate_t, int, int);

void pv_crs_fini(pvstate_t);
void pv_crs_init(pvstate_t);
void pv_crs_update(pvstate_t, char *);
#ifdef HAVE_IPC
void pv_crs_needreinit(pvstate_t);
#endif

void pv_sig_allowpause(void);
void pv_sig_checkbg(void);
void pv_sig_nopause(void);

void pv_remote_init(pvstate_t);
void pv_remote_check(pvstate_t);
void pv_remote_fini(pvstate_t);
int pv_remote_set(pvstate_t);

#ifdef __cplusplus
}
#endif

#endif /* _PV_INTERNAL_H */

/* EOF */
