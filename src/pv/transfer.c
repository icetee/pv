/*
 * Functions for transferring between file descriptors.
 */

#include "pv-internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>


/*
 * Read up to "count" bytes from file descriptor "fd" into the buffer "buf",
 * and return the number of bytes read, like read().
 *
 * Unlike read(), if we have read less than "count" bytes, we check to see
 * if there's any more to read, and keep trying, to make sure we fill the
 * buffer as full as we can.
 */
static ssize_t pv__transfer_read_repeated(int fd, void *buf, size_t count)
{
	ssize_t total_read;

	total_read = 0;

	while (count > 0) {
		ssize_t nread;

		nread = read(fd, buf, count);
		if (nread < 0)
			return nread;

		total_read += nread;
		buf += nread;
		count -= nread;

		if (0 == nread)
			return total_read;

		if (count > 0) {
			fd_set readfds;
			struct timeval tv;

			tv.tv_sec = 0;
			tv.tv_usec = 0;
			FD_ZERO(&readfds);
			FD_SET(fd, &readfds);

			debug("%s %d: %s (%ld %s, %ld %s)", "fd", fd,
			      "trying another read after partial buffer fill",
			      nread, "read", count, "remaining");

			if (select(fd + 1, &readfds, NULL, NULL, &tv) < 1)
				break;
		}
	}

	return total_read;
}


/*
 * Write up to "count" bytes to file descriptor "fd" from the buffer "buf",
 * and return the number of bytes written, like write().
 *
 * Unlike write(), if we have written less than "count" bytes, we check to
 * see if we can write any more, and keep trying, to make sure we empty the
 * buffer as much as we can.
 */
static ssize_t pv__transfer_write_repeated(int fd, void *buf, size_t count)
{
	ssize_t total_written;

	total_written = 0;

	while (count > 0) {
		ssize_t nwritten;

		nwritten = write(fd, buf, count);
		if (nwritten < 0)
			return nwritten;

		total_written += nwritten;
		buf += nwritten;
		count -= nwritten;

		if (0 == nwritten)
			return total_written;

		if (count > 0) {
			fd_set writefds;
			struct timeval tv;

			tv.tv_sec = 0;
			tv.tv_usec = 0;
			FD_ZERO(&writefds);
			FD_SET(fd, &writefds);

			debug("%s %d: %s (%ld %s, %ld %s)", "fd", fd,
			      "trying another write after partial buffer flush",
			      nwritten, "written", count, "remaining");

			if (select(fd + 1, NULL, &writefds, NULL, &tv) < 1)
				break;
		}
	}

	return total_written;
}


/*
 * Read some data from the given file descriptor. Returns zero if there was
 * a transient error and we need to return 0 from pv_transfer, otherwise
 * returns 1.
 *
 * At most, the number of bytes read will be the number of bytes remaining
 * in the input buffer.  If state->rate_limit is >0, and/or "allowed" is >0,
 * then the maximum number of bytes read will be the number remaining unused
 * in the input buffer or the value of "allowed", whichever is smaller.
 *
 * If splice() was successfully used, sets state->splice_used to 1; if it
 * failed, then state->splice_failed_fd is updated to the current fd so
 * splice() won't be tried again until the next input file.
 *
 * Updates state->read_position by the number of bytes read, unless splice()
 * was used, in which case it does not since there's nothing in the buffer
 * (and it also adds the bytes to state->written since they've been written
 * to the output).
 *
 * On read error, updates state->exit_status, and if allowed by
 * state->skip_errors, tries to skip past the problem.
 *
 * If the end of the input file is reached or the error is unrecoverable,
 * sets *eof_in to 1.  If all data in the buffer has been written at this
 * point, then also sets *eof_out.
 */
static int pv__transfer_read(pvstate_t state, int fd,
			     int *eof_in, int *eof_out,
			     unsigned long long allowed,
			     long *lineswritten)
{
	unsigned long bytes_can_read;
	unsigned long amount_to_skip;
	long amount_skipped;
	long orig_offset;
	long skip_offset;
	ssize_t nread;
#ifdef HAVE_SPLICE
	size_t bytes_to_splice;
#endif				/* HAVE_SPLICE */

	bytes_can_read = state->buffer_size - state->read_position;

#ifdef HAVE_SPLICE
	state->splice_used = 0;
	if ((!state->linemode) && (!state->no_splice)
	    && (fd != state->splice_failed_fd)
	    && (0 == state->to_write)) {
		if (state->rate_limit || allowed != 0)
			bytes_to_splice = allowed;
		else
			bytes_to_splice = bytes_can_read;

		nread = splice(fd, NULL, STDOUT_FILENO, NULL,
			       bytes_to_splice, SPLICE_F_MORE);

		state->splice_used = 1;
		if ((nread < 0) && (EINVAL == errno)) {
			debug("%s %d: %s", "fd", fd,
			      "splice failed with EINVAL - disabling");
			state->splice_failed_fd = fd;
			state->splice_used = 0;
			/*
			 * Fall through to read() below.
			 */
		} else if (nread > 0) {
			state->written = nread;
		} else if ((-1 == nread) && (EAGAIN == errno)) {
			/* nothing read yet - do nothing */
		} else {
			/* EOF might not really be EOF, it seems */
			state->splice_used = 0;
		}
	}
	if (0 == state->splice_used) {
		nread =
		    pv__transfer_read_repeated(fd,
					       state->transfer_buffer +
					       state->read_position,
					       bytes_can_read);
	}
#else
	nread =
	    pv__transfer_read_repeated(fd,
				       state->transfer_buffer +
				       state->read_position,
				       bytes_can_read);
#endif				/* HAVE_SPLICE */


	if (0 == nread) {
		/*
		 * If read returned 0, we've reached the end of this input
		 * file.  If we've also written all the data in the transfer
		 * buffer, we set eof_out as well, so that the main loop can
		 * move on to the next input file.
		 */
		*eof_in = 1;
		if (state->write_position >= state->read_position)
			*eof_out = 1;
		return 1;
	} else if (nread > 0) {
		/*
		 * Read returned >0, so we successfully read data - clear
		 * the error counter and update our record of how much data
		 * we've got in the buffer.
		 */
		state->read_errors_in_a_row = 0;
#ifdef HAVE_SPLICE
		/*
		 * If we used splice(), there isn't any more data in the
		 * buffer than there was before.
		 */
		if (0 == state->splice_used)
			state->read_position += nread;
#else
		state->read_position += nread;
#endif				/* HAVE_SPLICE */
		return 1;
	}

	/*
	 * If we reach this point, nread<0, so there was an error.
	 */

	/*
	 * If a read error occurred but it was EINTR or EAGAIN, just wait a
	 * bit and then return zero, since this was a transient error.
	 */
	if ((EINTR == errno) || (EAGAIN == errno)) {
		struct timeval tv;
		debug("%s %d: %s: %s", "fd", fd,
		      "transient error - waiting briefly",
		      strerror(errno));
		tv.tv_sec = 0;
		tv.tv_usec = 10000;
		select(0, NULL, NULL, NULL, &tv);
		return 0;
	}

	/*
	 * The read error is not transient, so update the program's final
	 * exit status, regardless of whether we're skipping errors, and
	 * increment the error counter.
	 */
	state->exit_status |= 16;
	state->read_errors_in_a_row++;

	/*
	 * If we aren't skipping errors, show the error and pretend we
	 * reached the end of this file.
	 */
	if (0 == state->skip_errors) {
		pv_error(state, "%s: %s: %s",
			 state->current_file,
			 _("read failed"), strerror(errno));
		*eof_in = 1;
		if (state->write_position >= state->read_position) {
			*eof_out = 1;
		}
		return 1;
	}

	/*
	 * Try to skip past the error.
	 */

	amount_skipped = -1;

	if (!state->read_error_warning_shown) {
		pv_error(state, "%s: %s: %s",
			 state->current_file,
			 _
			 ("warning: read errors detected"),
			 strerror(errno));
		state->read_error_warning_shown = 1;
	}

	orig_offset = lseek64(fd, 0, SEEK_CUR);

	/*
	 * If the file is not seekable, we can't skip past the error, so we
	 * will have to abandon the attempt and pretend we reached the end
	 * of the file.
	 */
	if (0 > orig_offset) {
		pv_error(state, "%s: %s: %s",
			 state->current_file,
			 _("file is not seekable"), strerror(errno));
		*eof_in = 1;
		if (state->write_position >= state->read_position) {
			*eof_out = 1;
		}
		return 1;
	}

	if (state->read_errors_in_a_row < 10) {
		amount_to_skip = state->read_errors_in_a_row < 5 ? 1 : 2;
	} else if (state->read_errors_in_a_row < 20) {
		amount_to_skip = 1 << (state->read_errors_in_a_row - 10);
	} else {
		amount_to_skip = 512;
	}

	/*
	 * Round the skip amount down to the start of the next block of the
	 * skip amount size.  For instance if the skip amount is 512, but
	 * our file offset is 257, we'll jump to 512 instead of 769.
	 */
	if (amount_to_skip > 1) {
		skip_offset = orig_offset + amount_to_skip;
		skip_offset -= (skip_offset % amount_to_skip);
		if (skip_offset > orig_offset) {
			amount_to_skip = skip_offset - orig_offset;
		}
	}

	/*
	 * Trim the skip amount so we wouldn't read too much.
	 */
	if (amount_to_skip > bytes_can_read)
		amount_to_skip = bytes_can_read;

	skip_offset = lseek64(fd, orig_offset + amount_to_skip, SEEK_SET);

	/*
	 * If the skip we just tried didn't work, try only skipping 1 byte
	 * in case we were trying to go past the end of the input file.
	 */
	if (skip_offset < 0) {
		amount_to_skip = 1;
		skip_offset =
		    lseek64(fd, orig_offset + amount_to_skip, SEEK_SET);
	}

	if (skip_offset < 0) {
		/*
		 * Failed to skip - lseek() returned an error, so mark the
		 * file as having ended.
		 */
		*eof_in = 1;
		/*
		 * EINVAL means the file has ended since we've tried to go
		 * past the end of it, so we don't bother with a warning
		 * since it just means we've reached the end anyway.
		 */
		if (EINVAL != errno) {
			pv_error(state,
				 "%s: %s: %s",
				 state->current_file,
				 _
				 ("failed to seek past error"),
				 strerror(errno));
		}
	} else {
		amount_skipped = skip_offset - orig_offset;
	}

	/*
	 * If we succeeded in skipping some bytes, zero the equivalent part
	 * of the transfer buffer, and update the buffer position.
	 */
	if (amount_skipped > 0) {
		memset(state->transfer_buffer +
		       state->read_position, 0, amount_skipped);
		state->read_position += amount_skipped;
		if (state->skip_errors < 2) {
			pv_error(state, "%s: %s: %ld - %ld (%ld %s)",
				 state->current_file,
				 _
				 ("skipped past read error"),
				 orig_offset,
				 skip_offset, amount_skipped, _("B"));
		}
	} else {
		/*
		 * Failed to skip - mark file as ended.
		 */
		*eof_in = 1;
		if (state->write_position >= state->read_position) {
			*eof_out = 1;
		}
	}

	return 1;
}


/*
 * Write state->to_write bytes of data from the transfer buffer to stdout.
 * Returns zero if there was a transient error and we need to return 0 from
 * pv_transfer, otherwise returns 1.
 *
 * Updates state->write_position by moving it on by the number of bytes
 * written; adds the number of bytes written to state->written; sets
 * *eof_out on stdout EOF or when the write position catches up with the
 * read position AND *eof_in is 1 (meaning we've reached the end of data).
 *
 * On error, sets *eof_out to 1, sets state->written to -1, and updates
 * state->exit_status.
 */
static int pv__transfer_write(pvstate_t state, int fd,
			      int *eof_in, int *eof_out,
			      long *lineswritten)
{
	ssize_t nwritten;

	signal(SIGALRM, SIG_IGN);
	alarm(1);

	nwritten = pv__transfer_write_repeated(STDOUT_FILENO,
					       state->transfer_buffer +
					       state->write_position,
					       state->to_write);

	alarm(0);

	if (0 == nwritten) {
		/*
		 * Write returned 0 - EOF on stdout.
		 */
		*eof_out = 1;
		return 1;
	} else if (nwritten > 0) {
		/*
		 * Write returned >0 - data successfully written.
		 */
		if ((state->linemode) && (lineswritten != NULL)) {
			/*
			 * Guillaume Marcais: use strchr to count \n
			 */
			unsigned char save;
			char *ptr;
			long lines = 0;

			save =
			    state->transfer_buffer[state->write_position +
						   nwritten];
			state->transfer_buffer[state->write_position +
					       nwritten] = 0;
			ptr =
			    (char *) (state->transfer_buffer +
				      state->write_position - 1);

			if (state->null) {
				for (ptr++;
				     ptr -
				     (char *) state->transfer_buffer -
				     state->write_position < nwritten;
				     ptr++) {
					if (*ptr == '\0')
						++lines;
				}
			} else {
				while ((ptr =
					strchr((char *) (ptr + 1), '\n')))
					++lines;
			}

			*lineswritten += lines;
			state->transfer_buffer[state->write_position +
					       nwritten] = save;
		}

		state->write_position += nwritten;
		state->written += nwritten;

		/*
		 * If we're monitoring the output, update our copy of the
		 * last few bytes we've written.
		 */
		if (((state->components_used & PV_DISPLAY_OUTPUTBUF) != 0)
		    && (nwritten > 0)) {
			long new_portion_length, old_portion_length;

			new_portion_length = nwritten;
			if (new_portion_length > state->lastoutput_length)
				new_portion_length =
				    state->lastoutput_length;

			old_portion_length =
			    state->lastoutput_length - new_portion_length;

			/*
			 * Make room for the new portion.
			 */
			if (old_portion_length > 0) {
				memmove(state->lastoutput_buffer,
					state->lastoutput_buffer +
					new_portion_length,
					old_portion_length);
			}

			/*
			 * Copy the new data in.
			 */
			memcpy(state->lastoutput_buffer +
			       old_portion_length,
			       state->transfer_buffer +
			       state->write_position - new_portion_length,
			       new_portion_length);
		}

		/*
		 * If we've written all the data in the buffer, reset the
		 * read pointer to the start, and if the input file is at
		 * EOF, set eof_out as well to indicate that we've written
		 * everything for this input file.
		 */
		if (state->write_position >= state->read_position) {
			state->write_position = 0;
			state->read_position = 0;
			if (*eof_in)
				*eof_out = 1;
		}

		return 1;
	}

	/*
	 * If we reach this point, nwritten<0, so there was an error.
	 */

	/*
	 * If a write error occurred but it was EINTR or EAGAIN, just wait a
	 * bit and then return zero, since this was a transient error.
	 */
	if ((EINTR == errno) || (EAGAIN == errno)) {
		struct timeval tv;
		tv.tv_sec = 0;
		tv.tv_usec = 10000;
		select(0, NULL, NULL, NULL, &tv);
		return 0;
	}

	/*
	 * SIGPIPE means we've finished. Don't output an error because it's
	 * not really our error to report.
	 */
	if (EPIPE == errno) {
		*eof_in = 1;
		*eof_out = 1;
		return 0;
	}

	pv_error(state, "%s: %s", _("write failed"), strerror(errno));
	state->exit_status |= 16;
	*eof_out = 1;
	state->written = -1;

	return 1;
}


/*
 * Transfer some data from "fd" to standard output, timing out after 9/100
 * of a second.  If state->rate_limit is >0, and/or "allowed" is >0, only up
 * to "allowed" bytes can be written.  The variables that "eof_in" and
 * "eof_out" point to are used to flag that we've finished reading and
 * writing respectively.
 *
 * Returns the number of bytes written, or negative on error (in which case
 * state->exit_status is updated). In line mode, the number of lines written
 * will be put into *lineswritten.
 */
long pv_transfer(pvstate_t state, int fd, int *eof_in, int *eof_out,
		 unsigned long long allowed, long *lineswritten)
{
	struct timeval tv;
	fd_set readfds;
	fd_set writefds;
	int max_fd;
	int n;

	if (NULL == state)
		return 0;

	/*
	 * Reinitialise the error skipping variables if the file descriptor
	 * has changed since the last time we were called.
	 */
	if (fd != state->last_read_skip_fd) {
		state->last_read_skip_fd = fd;
		state->read_errors_in_a_row = 0;
		state->read_error_warning_shown = 0;
	}

	if (NULL == state->transfer_buffer) {
		state->buffer_size = state->target_buffer_size;
		state->transfer_buffer =
		    (unsigned char *) malloc(state->buffer_size + 32);
		if (NULL == state->transfer_buffer) {
			pv_error(state, "%s: %s",
				 _("buffer allocation failed"),
				 strerror(errno));
			state->exit_status |= 64;
			return -1;
		}
	}

	/*
	 * Reallocate the buffer if the buffer size has changed mid-transfer.
	 */
	if (state->buffer_size < state->target_buffer_size) {
		unsigned char *newptr;
		newptr =
		    realloc(state->transfer_buffer,
			    state->target_buffer_size + 32);
		if (NULL == newptr) {
			/*
			 * Reset target if realloc failed so we don't keep
			 * trying to realloc over and over.
			 */
			debug("realloc: %s", strerror(errno));
			state->target_buffer_size = state->buffer_size;
		} else {
			debug("%s: %ld", "buffer resized",
			      state->buffer_size);
			state->transfer_buffer = newptr;
			state->buffer_size = state->target_buffer_size;
		}
	}

	if ((state->linemode) && (lineswritten != NULL))
		*lineswritten = 0;

	if ((*eof_in) && (*eof_out))
		return 0;

	tv.tv_sec = 0;
	tv.tv_usec = 90000;

	FD_ZERO(&readfds);
	FD_ZERO(&writefds);

	max_fd = 0;

	/*
	 * If the input file is not at EOF and there's room in the buffer,
	 * look for incoming data from it.
	 */
	if ((!(*eof_in)) && (state->read_position < state->buffer_size)) {
		FD_SET(fd, &readfds);
		if (fd > max_fd)
			max_fd = fd;
	}

	/*
	 * Work out how much we're allowed to write, based on the amount of
	 * data left in the buffer.  If rate limiting is active or "allowed"
	 * is >0, then this puts an upper limit on how much we're allowed to
	 * write.
	 */
	state->to_write = state->read_position - state->write_position;
	if ((state->rate_limit > 0) || (allowed > 0)) {
		if (state->to_write > allowed) {
			state->to_write = allowed;
		}
	}

	/*
	 * If we don't think we've finished writing and there's anything
	 * we're allowed to write, look for the stdout becoming writable.
	 */
	if ((!(*eof_out)) && (state->to_write > 0)) {
		FD_SET(STDOUT_FILENO, &writefds);
		if (STDOUT_FILENO > max_fd)
			max_fd = STDOUT_FILENO;
	}

	n = select(max_fd + 1, &readfds, &writefds, NULL, &tv);

	if (n < 0) {
		/*
		 * Ignore transient errors by returning 0 immediately.
		 */
		if (EINTR == errno)
			return 0;

		/*
		 * Any other error is a problem and we must report back.
		 */
		pv_error(state, "%s: %s: %d: %s",
			 state->current_file,
			 _("select call failed"), n, strerror(errno));

		state->exit_status |= 16;

		return -1;
	}

	state->written = 0;

	/*
	 * If there is data to read, try to read some in. Return early if
	 * there was a transient read error.
	 *
	 * NB this can update state->written because of splice().
	 */
	if (FD_ISSET(fd, &readfds)) {
		if (pv__transfer_read
		    (state, fd, eof_in, eof_out, allowed,
		     lineswritten) == 0)
			return 0;
	}

	/*
	 * In line mode, only write up to and including the last newline,
	 * so that we're writing output line-by-line.
	 */
	if ((state->to_write > 0) && (state->linemode) && !(state->null)) {
		/*
		 * Guillaume Marcais: use strrchr to find last \n
		 */
		unsigned char save;
		char *start;
		char *end;

		save =
		    state->transfer_buffer[state->write_position +
					   state->to_write];
		state->transfer_buffer[state->write_position +
				       state->to_write] = 0;

		start =
		    (char *) (state->transfer_buffer +
			      state->write_position);
		end = strrchr(start, '\n');
		state->transfer_buffer[state->write_position +
				       state->to_write] = save;

		if (end != NULL) {
			state->to_write = (end - start) + 1;
		}
	}

	/*
	 * If there is data to write, and stdout is ready to receive it, and
	 * we didn't use splice() this time, write some data.  Return early
	 * if there was a transient write error.
	 */
	if (FD_ISSET(STDOUT_FILENO, &writefds)
#ifdef HAVE_SPLICE
	    && (0 == state->splice_used)
#endif				/* HAVE_SPLICE */
	    && (state->read_position > state->write_position)
	    && (state->to_write > 0)) {
		if (pv__transfer_write
		    (state, fd, eof_in, eof_out, lineswritten) == 0)
			return 0;
	}
#ifdef MAXIMISE_BUFFER_FILL
	/*
	 * Rotate the written bytes out of the buffer so that it can be
	 * filled up completely by the next read.
	 */
	if (state->write_position > 0) {
		if (state->write_position < state->read_position) {
			memmove(state->transfer_buffer,
				state->transfer_buffer +
				state->write_position,
				state->read_position -
				state->write_position);
			state->read_position -= state->write_position;
			state->write_position = 0;
		} else {
			state->write_position = 0;
			state->read_position = 0;
		}
	}
#endif				/* MAXIMISE_BUFFER_FILL */

	return state->written;
}

/* EOF */
