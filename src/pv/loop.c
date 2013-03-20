/*
 * Main program entry point - read the command line options, then perform
 * the appropriate actions.
 *
 * Copyright 2013 Andrew Wood, distributed under the Artistic License 2.0.
 */

#include "pv-internal.h"

#define _GNU_SOURCE 1
#include <limits.h>

#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/stat.h>


/*
 * Add the given number of microseconds (which may be negative) to the given
 * timeval.
 */
static void pv_timeval_add_usec(struct timeval *val, long usec)
{
	val->tv_usec += usec;
	while (val->tv_usec < 0) {
		val->tv_sec--;
		val->tv_usec += 1000000;
	}
	while (val->tv_usec >= 1000000) {
		val->tv_sec++;
		val->tv_usec -= 1000000;
	}
}


/*
 * Pipe data from a list of files to standard output, giving information
 * about the transfer on standard error according to the given options.
 *
 * Returns nonzero on error.
 */
int pv_main_loop(pvstate_t state)
{
	long written, lineswritten;
	long long total_written, since_last, cansend;
	long double target;
	int eof_in, eof_out, final_update;
	struct timeval start_time, next_update, next_ratecheck, cur_time;
	struct timeval init_time, next_remotecheck;
	long double elapsed;
	struct stat64 sb;
	int fd, n;

	/*
	 * "written" is ALWAYS bytes written by the last transfer.
	 *
	 * "lineswritten" is the lines written by the last transfer,
	 * but is only updated in line mode.
	 *
	 * "total_written" is the total bytes written since the start,
	 * or in line mode, the total lines written since the start.
	 *
	 * "since_last" is the bytes written since the last display,
	 * or in line mode, the lines written since the last display.
	 *
	 * The remaining variables are all unchanged by linemode.
	 */

	fd = -1;

	pv_crs_init(state);

	eof_in = 0;
	eof_out = 0;
	total_written = 0;
	since_last = 0;

	gettimeofday(&start_time, NULL);
	gettimeofday(&cur_time, NULL);

	next_update.tv_sec = start_time.tv_sec;
	next_update.tv_usec = start_time.tv_usec;
	pv_timeval_add_usec(&next_update,
			    (long) (1000000.0 * state->interval));

	next_ratecheck.tv_sec = start_time.tv_sec;
	next_ratecheck.tv_usec = start_time.tv_usec;
	next_remotecheck.tv_sec = start_time.tv_sec;
	next_remotecheck.tv_usec = start_time.tv_usec;

	target = 0;
	final_update = 0;
	n = 0;

	fd = pv_next_file(state, n, -1);
	if (fd < 0) {
		if (state->cursor)
			pv_crs_fini(state);
		return state->exit_status;
	}

	/*
	 * Set target buffer size if the initial file's block size can be
	 * read and we weren't given a target buffer size.
	 */
	if (0 == fstat64(fd, &sb)) {
		unsigned long long sz;
		sz = sb.st_blksize * 32;
		if (sz > BUFFER_SIZE_MAX)
			sz = BUFFER_SIZE_MAX;
		state->target_buffer_size = sz;
	}

	if (0 == state->target_buffer_size)
		state->target_buffer_size = BUFFER_SIZE;

	while ((!(eof_in && eof_out)) || (!final_update)) {

		cansend = 0;

		/*
		 * Check for remote messages from -R every short while
		 */
		if ((cur_time.tv_sec > next_remotecheck.tv_sec)
		    || (cur_time.tv_sec == next_remotecheck.tv_sec
			&& cur_time.tv_usec >= next_remotecheck.tv_usec)) {
			pv_remote_check(state);
			pv_timeval_add_usec(&next_remotecheck,
					    REMOTE_INTERVAL);
		}

		if (state->pv_sig_abort)
			break;

		if (state->rate_limit > 0) {
			gettimeofday(&cur_time, NULL);
			if ((cur_time.tv_sec > next_ratecheck.tv_sec)
			    || (cur_time.tv_sec == next_ratecheck.tv_sec
				&& cur_time.tv_usec >=
				next_ratecheck.tv_usec)) {
				target +=
				    ((long double) (state->rate_limit)) /
				    (long double) (1000000 /
						   RATE_GRANULARITY);
				pv_timeval_add_usec(&next_ratecheck,
						    RATE_GRANULARITY);
			}
			cansend = target;
		}

		/*
		 * If we have to stop at "size" bytes, make sure we don't
		 * try to write more than we're allowed to.
		 */
		if ((0 < state->size) && (state->stop_at_size)) {
			if ((state->size < (total_written + cansend))
			    || ((0 == cansend)
				&& (0 == state->rate_limit))) {
				cansend = state->size - total_written;
				if (0 >= cansend) {
					eof_in = 1;
					eof_out = 1;
				}
			}
		}

		if ((0 < state->size) && (state->stop_at_size)
		    && (0 >= cansend) && eof_in && eof_out) {
			written = 0;
		} else {
			written =
			    pv_transfer(state, fd, &eof_in, &eof_out,
					cansend, &lineswritten);
		}

		if (written < 0) {
			if (state->cursor)
				pv_crs_fini(state);
			return state->exit_status;
		}

		if (state->linemode) {
			since_last += lineswritten;
			total_written += lineswritten;
			if (state->rate_limit > 0)
				target -= lineswritten;
		} else {
			since_last += written;
			total_written += written;
			if (state->rate_limit > 0)
				target -= written;
		}

		if (eof_in && eof_out && n < (state->input_file_count - 1)) {
			n++;
			fd = pv_next_file(state, n, fd);
			if (fd < 0) {
				if (state->cursor)
					pv_crs_fini(state);
				return state->exit_status;
			}
			eof_in = 0;
			eof_out = 0;
		}

		gettimeofday(&cur_time, NULL);

		if (eof_in && eof_out) {
			final_update = 1;
			next_update.tv_sec = cur_time.tv_sec - 1;
		}

		if (state->no_op)
			continue;

		/*
		 * If -W was given, we don't output anything until we have
		 * written a byte (or line, in line mode), at which point
		 * we then count time as if we started when the first byte
		 * was received.
		 */
		if (state->wait) {
			if (state->linemode) {
				if (lineswritten < 1)
					continue;
			} else {
				if (written < 1)
					continue;
			}

			state->wait = 0;

			/*
			 * Reset the timer offset counter now that data
			 * transfer has begun, otherwise if we had been
			 * stopped and started (with ^Z / SIGTSTOP)
			 * previously (while waiting for data), the timers
			 * will be wrongly offset.
			 *
			 * While we reset the offset counter we must disable
			 * SIGTSTOP so things don't mess up.
			 */
			pv_sig_nopause();
			gettimeofday(&start_time, NULL);
			state->pv_sig_toffset.tv_sec = 0;
			state->pv_sig_toffset.tv_usec = 0;
			pv_sig_allowpause();

			next_update.tv_sec = start_time.tv_sec;
			next_update.tv_usec = start_time.tv_usec;
			pv_timeval_add_usec(&next_update,
					    (long) (1000000.0 *
						    state->interval));
		}

		if ((cur_time.tv_sec < next_update.tv_sec)
		    || (cur_time.tv_sec == next_update.tv_sec
			&& cur_time.tv_usec < next_update.tv_usec)) {
			continue;
		}

		pv_timeval_add_usec(&next_update,
				    (long) (1000000.0 * state->interval));

		if (next_update.tv_sec < cur_time.tv_sec) {
			next_update.tv_sec = cur_time.tv_sec;
			next_update.tv_usec = cur_time.tv_usec;
		} else if (next_update.tv_sec == cur_time.tv_sec
			   && next_update.tv_usec < cur_time.tv_usec) {
			next_update.tv_usec = cur_time.tv_usec;
		}

		init_time.tv_sec =
		    start_time.tv_sec + state->pv_sig_toffset.tv_sec;
		init_time.tv_usec =
		    start_time.tv_usec + state->pv_sig_toffset.tv_usec;
		if (init_time.tv_usec >= 1000000) {
			init_time.tv_sec++;
			init_time.tv_usec -= 1000000;
		}
		if (init_time.tv_usec < 0) {
			init_time.tv_sec--;
			init_time.tv_usec += 1000000;
		}

		elapsed = cur_time.tv_sec - init_time.tv_sec;
		elapsed +=
		    (cur_time.tv_usec - init_time.tv_usec) / 1000000.0;

		if (final_update)
			since_last = -1;

		if (state->pv_sig_newsize) {
			state->pv_sig_newsize = 0;
			pv_screensize(&(state->width), &(state->height));
		}

		pv_display(state, elapsed, since_last, total_written);

		since_last = 0;
	}

	if (state->cursor) {
		pv_crs_fini(state);
	} else {
		if ((!state->numeric) && (!state->no_op))
			write(STDERR_FILENO, "\n", 1);
	}

	if (state->pv_sig_abort)
		state->exit_status |= 32;

	return state->exit_status;
}

/* EOF */
