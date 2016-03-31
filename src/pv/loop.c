/*
 * Main program entry point - read the command line options, then perform
 * the appropriate actions.
 */

#include "pv-internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

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
	state->initial_offset = 0;

	gettimeofday(&start_time, NULL);
	gettimeofday(&cur_time, NULL);

	next_update.tv_sec = start_time.tv_sec;
	next_update.tv_usec = start_time.tv_usec;
	if ((state->delay_start > 0)
	    && (state->delay_start > state->interval)) {
		pv_timeval_add_usec(&next_update,
				    (long) (1000000.0 *
					    state->delay_start));
	} else {
		pv_timeval_add_usec(&next_update,
				    (long) (1000000.0 * state->interval));
	}

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
			if ((state->display_visible)
			    || (0 == state->delay_start))
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
		if ((!state->numeric) && (!state->no_op)
		    && (state->display_visible))
			write(STDERR_FILENO, "\n", 1);
	}

	if (state->pv_sig_abort)
		state->exit_status |= 32;

	if (fd >= 0)
		close(fd);

	return state->exit_status;
}


/*
 * Watch the progress of file descriptor state->watch_fd in process
 * state->watch_pid and show details about the transfer on standard error
 * according to the given options.
 *
 * Returns nonzero on error.
 */
int pv_watchfd_loop(pvstate_t state)
{
	struct pvwatchfd_s info;
	long long position_now, total_written, since_last;
	struct timeval next_update, cur_time;
	struct timeval init_time, next_remotecheck;
	long double elapsed;
	int ended;
	int first_check;
	int rc;

	info.watch_pid = state->watch_pid;
	info.watch_fd = state->watch_fd;
	rc = pv_watchfd_info(state, &info, 0);
	if (0 != rc) {
		state->exit_status |= 2;
		return state->exit_status;
	}

	/*
	 * Use a size if one was passed, otherwise use the total size
	 * calculated.
	 */
	if (0 >= state->size)
		state->size = info.size;

	if (state->size < 1) {
		char *fmt;
		while (NULL != (fmt = strstr(state->default_format, "%e"))) {
			debug("%s", "zero size - removing ETA");
			/* strlen-1 here to include trailing NUL */
			memmove(fmt, fmt + 2, strlen(fmt) - 1);
			state->reparse_display = 1;
		}
	}

	gettimeofday(&(info.start_time), NULL);
	gettimeofday(&cur_time, NULL);

	next_update.tv_sec = info.start_time.tv_sec;
	next_update.tv_usec = info.start_time.tv_usec;
	pv_timeval_add_usec(&next_update,
			    (long) (1000000.0 * state->interval));

	next_remotecheck.tv_sec = info.start_time.tv_sec;
	next_remotecheck.tv_usec = info.start_time.tv_usec;

	ended = 0;
	total_written = 0;
	since_last = 0;
	first_check = 1;

	while (!ended) {
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

		position_now = pv_watchfd_position(&info);

		if (position_now < 0) {
			ended = 1;
		} else {
			since_last += position_now - total_written;
			total_written = position_now;
			if (first_check) {
				state->initial_offset = position_now;
				first_check = 0;
			}
		}

		gettimeofday(&cur_time, NULL);

		if (ended) {
			ended = 1;
			next_update.tv_sec = cur_time.tv_sec - 1;
		}

		if ((cur_time.tv_sec < next_update.tv_sec)
		    || (cur_time.tv_sec == next_update.tv_sec
			&& cur_time.tv_usec < next_update.tv_usec)) {
			struct timeval tv;
			tv.tv_sec = 0;
			tv.tv_usec = 50000;
			select(0, NULL, NULL, NULL, &tv);
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
		    info.start_time.tv_sec + state->pv_sig_toffset.tv_sec;
		init_time.tv_usec =
		    info.start_time.tv_usec +
		    state->pv_sig_toffset.tv_usec;
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

		if (ended)
			since_last = -1;

		if (state->pv_sig_newsize) {
			state->pv_sig_newsize = 0;
			pv_screensize(&(state->width), &(state->height));
		}

		pv_display(state, elapsed, since_last, total_written);

		since_last = 0;
	}

	if (!state->numeric)
		write(STDERR_FILENO, "\n", 1);

	if (state->pv_sig_abort)
		state->exit_status |= 32;

	return state->exit_status;
}


/*
 * Watch the progress of all file descriptors in process state->watch_pid
 * and show details about the transfers on standard error according to the
 * given options.
 *
 * Returns nonzero on error.
 */
int pv_watchpid_loop(pvstate_t state)
{
	struct pvstate_s state_copy;
	const char *original_format_string;
	char new_format_string[512] = { 0, };
	struct pvwatchfd_s *info_array = NULL;
	struct pvstate_s *state_array = NULL;
	int array_length = 0;
	int fd_to_idx[FD_SETSIZE] = { 0, };
	struct timeval next_update, cur_time, next_remotecheck;
	int idx;
	int prev_displayed_lines, blank_lines;

	/*
	 * Make sure the process exists first, so we can give an error if
	 * it's not there at the start.
	 */
	if (kill(state->watch_pid, 0) != 0) {
		pv_error(state, "%s %u: %s",
			 _("pid"), state->watch_pid, strerror(errno));
		state->exit_status |= 2;
		return 2;
	}

	/*
	 * Make a copy of our state, ready to change in preparation for
	 * duplication.
	 */
	memcpy(&state_copy, state, sizeof(state_copy));

	/*
	 * Make sure there's a format string, and then insert %N into it if
	 * it's not present.
	 */
	original_format_string =
	    state->format_string ? state->format_string : state->
	    default_format;
	if (NULL == strstr(original_format_string, "%N")) {
		snprintf(new_format_string, sizeof(new_format_string) - 1,
			 "%%N %s", original_format_string);
	} else {
		snprintf(new_format_string, sizeof(new_format_string) - 1,
			 "%s", original_format_string);
	}
	state_copy.format_string = NULL;
	snprintf(state_copy.default_format,
		 sizeof(state_copy.default_format) - 1, "%s",
		 new_format_string);

	/*
	 * Get things ready for the main loop.
	 */

	gettimeofday(&cur_time, NULL);

	next_remotecheck.tv_sec = cur_time.tv_sec;
	next_remotecheck.tv_usec = cur_time.tv_usec;

	next_update.tv_sec = cur_time.tv_sec;
	next_update.tv_usec = cur_time.tv_usec;
	pv_timeval_add_usec(&next_update,
			    (long) (1000000.0 * state->interval));

	for (idx = 0; idx < FD_SETSIZE; idx++) {
		fd_to_idx[idx] = -1;
	}

	prev_displayed_lines = 0;

	while (1) {
		int rc, fd, displayed_lines;

		if (state->pv_sig_abort)
			break;

		gettimeofday(&cur_time, NULL);

		if (kill(state->watch_pid, 0) != 0)
			break;

		if ((cur_time.tv_sec < next_update.tv_sec)
		    || (cur_time.tv_sec == next_update.tv_sec
			&& cur_time.tv_usec < next_update.tv_usec)) {
			struct timeval tv;
			tv.tv_sec = 0;
			tv.tv_usec = 50000;
			select(0, NULL, NULL, NULL, &tv);
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

		if (state->pv_sig_newsize) {
			state->pv_sig_newsize = 0;
			pv_screensize(&(state->width), &(state->height));
			for (idx = 0; idx < array_length; idx++) {
				state_array[idx].width = state->width;
				state_array[idx].height = state->height;
				pv_watchpid_setname(state,
						    &(info_array[idx]));
				state_array[idx].reparse_display = 1;
			}
		}

		rc = pv_watchpid_scanfds(state, &state_copy,
					 state->watch_pid, &array_length,
					 &info_array, &state_array,
					 fd_to_idx);
		if (rc != 0)
			break;

		displayed_lines = 0;

		for (fd = 0; fd < FD_SETSIZE; fd++) {
			long long position_now, since_last;
			struct timeval init_time;
			long double elapsed;

			if (displayed_lines >= state->height)
				break;

			idx = fd_to_idx[fd];

			if (idx < 0)
				continue;

			if (info_array[idx].watch_fd < 0) {
				/*
				 * Non-displayable fd - just remove if
				 * changed
				 */
				if (pv_watchfd_changed(&(info_array[idx]))) {
					fd_to_idx[fd] = -1;
					info_array[idx].watch_pid = 0;
					debug("%s %d: %s", "fd", fd,
					      "removing");
				}
				continue;
			}

			/*
			 * Displayable fd - display, or remove if changed
			 */

			position_now =
			    pv_watchfd_position(&(info_array[idx]));

			if (position_now < 0) {
				fd_to_idx[fd] = -1;
				info_array[idx].watch_pid = 0;
				debug("%s %d: %s", "fd", fd, "removing");
				continue;
			}

			since_last =
			    position_now - info_array[idx].position;
			info_array[idx].position = position_now;

			init_time.tv_sec =
			    info_array[idx].start_time.tv_sec +
			    state->pv_sig_toffset.tv_sec;
			init_time.tv_usec =
			    info_array[idx].start_time.tv_usec +
			    state->pv_sig_toffset.tv_usec;
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
			    (cur_time.tv_usec -
			     init_time.tv_usec) / 1000000.0;

			if (displayed_lines > 0) {
				debug("%s", "adding newline");
				write(STDERR_FILENO, "\n", 1);
			}

			debug("%s %d [%d]: %Lf / %Ld / %Ld", "fd", fd, idx,
			      elapsed, since_last, position_now);

			pv_display(&(state_array[idx]), elapsed,
				   since_last, position_now);
			displayed_lines++;
		}

		/*
		 * Write blank lines if we're writing fewer lines than last
		 * time.
		 */
		blank_lines = prev_displayed_lines - displayed_lines;
		prev_displayed_lines = displayed_lines;

		if (blank_lines > 0)
			debug("%s: %d", "adding blank lines", blank_lines);

		while (blank_lines > 0) {
			int x;
			if (displayed_lines > 0)
				write(STDERR_FILENO, "\n", 1);
			for (x = 0; x < state->width; x++)
				write(STDERR_FILENO, " ", 1);
			write(STDERR_FILENO, "\r", 1);
			blank_lines--;
			displayed_lines++;
		}

		debug("%s: %d", "displayed lines", displayed_lines);

		while (displayed_lines > 1) {
			write(STDERR_FILENO, "\033[A", 3);
			displayed_lines--;
		}
	}

	/*
	 * Clean up our displayed lines on exit.
	 */
	blank_lines = prev_displayed_lines;
	while (blank_lines > 0) {
		int x;
		for (x = 0; x < state->width; x++)
			write(STDERR_FILENO, " ", 1);
		write(STDERR_FILENO, "\r", 1);
		blank_lines--;
		if (blank_lines > 0)
			write(STDERR_FILENO, "\n", 1);
	}
	while (prev_displayed_lines > 1) {
		write(STDERR_FILENO, "\033[A", 3);
		prev_displayed_lines--;
	}

	if (NULL != info_array)
		free(info_array);
	if (NULL != state_array)
		free(state_array);

	return 0;
}

/* EOF */
