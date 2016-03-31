/*
 * Functions for opening and closing files.
 */

#include "pv-internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <limits.h>


/*
 * Try to work out the total size of all data by adding up the sizes of all
 * input files. If any of the input files are of indeterminate size (i.e.
 * they are a pipe), the total size is set to zero.
 *
 * Any files that cannot be stat()ed or that access() says we can't read
 * will cause a warning to be output and will be removed from the list.
 *
 * In line mode, any files that pass the above checks will then be read to
 * determine how many lines they contain, and the total size will be set to
 * the total line count. Only regular files will be read.
 *
 * Returns the total size, or 0 if it is unknown.
 */
unsigned long long pv_calc_total_size(pvstate_t state)
{
	unsigned long long total;
	struct stat64 sb;
	int rc, i, j, fd;

	total = 0;
	rc = 0;

	/*
	 * No files specified - check stdin.
	 */
	if (state->input_file_count < 1) {
		if (0 == fstat64(STDIN_FILENO, &sb))
			total = sb.st_size;
		return total;
	}

	for (i = 0; i < state->input_file_count; i++) {
		if (0 == strcmp(state->input_files[i], "-")) {
			rc = fstat64(STDIN_FILENO, &sb);
			if (rc != 0) {
				total = 0;
				return total;
			}
		} else {
			rc = stat64(state->input_files[i], &sb);
			if (0 == rc)
				rc = access(state->input_files[i], R_OK);
		}

		if (rc != 0) {
			pv_error(state, "%s: %s",
				 state->input_files[i], strerror(errno));
			for (j = i; j < state->input_file_count - 1; j++) {
				state->input_files[j] =
				    state->input_files[j + 1];
			}
			state->input_file_count--;
			i--;
			state->exit_status |= 2;
			continue;
		}

		if (S_ISBLK(sb.st_mode)) {
			/*
			 * Get the size of block devices by opening
			 * them and seeking to the end.
			 */
			if (0 == strcmp(state->input_files[i], "-")) {
				fd = open64("/dev/stdin", O_RDONLY);
			} else {
				fd = open64(state->input_files[i],
					    O_RDONLY);
			}
			if (fd >= 0) {
				total += lseek64(fd, 0, SEEK_END);
				close(fd);
			} else {
				pv_error(state, "%s: %s",
					 state->input_files[i],
					 strerror(errno));
				state->exit_status |= 2;
			}
		} else if (S_ISREG(sb.st_mode)) {
			total += sb.st_size;
		} else {
			total = 0;
		}
	}

	/*
	 * Patch from Peter Samuelson: if we cannot work out the size of the
	 * input, but we are writing to a block device, then use the size of
	 * the output block device.
	 *
	 * Further modified to check that stdout is not in append-only mode
	 * and that we can seek back to the start after getting the size.
	 */
	if (total <= 0) {
		rc = fstat64(STDOUT_FILENO, &sb);
		if ((0 == rc) && S_ISBLK(sb.st_mode)
		    && (0 == (fcntl(STDOUT_FILENO, F_GETFL) & O_APPEND))) {
			total = lseek64(STDOUT_FILENO, 0, SEEK_END);
			if (lseek64(STDOUT_FILENO, 0, SEEK_SET) != 0) {
				pv_error(state, "%s: %s: %s", "(stdout)",
					 _
					 ("failed to seek to start of output"),
					 strerror(errno));
				state->exit_status |= 2;
			}
			/*
			 * If we worked out a size, then set the
			 * stop-at-size flag to prevent a "no space left on
			 * device" error when we reach the end of the output
			 * device.
			 */
			if (total > 0) {
				state->stop_at_size = 1;
			}
		}
	}

	if (!state->linemode)
		return total;

	/*
	 * In line mode, we count input lines to work out the total size.
	 */
	total = 0;

	for (i = 0; i < state->input_file_count; i++) {
		fd = -1;

		if (0 == strcmp(state->input_files[i], "-")) {
			rc = fstat64(STDIN_FILENO, &sb);
			if ((rc != 0) || (!S_ISREG(sb.st_mode))) {
				total = 0;
				return total;
			}
			fd = dup(STDIN_FILENO);
		} else {
			rc = stat64(state->input_files[i], &sb);
			if ((rc != 0) || (!S_ISREG(sb.st_mode))) {
				total = 0;
				return total;
			}
			fd = open64(state->input_files[i], O_RDONLY);
		}

		if (fd < 0) {
			pv_error(state, "%s: %s", state->input_files[i],
				 strerror(errno));
			total = 0;
			state->exit_status |= 2;
			return total;
		}

		while (1) {
			unsigned char scanbuf[1024];
			int numread, j;

			numread = read(fd, scanbuf, sizeof(scanbuf));
			if (numread < 0) {
				pv_error(state, "%s: %s",
					 state->input_files[i],
					 strerror(errno));
				state->exit_status |= 2;
				break;
			} else if (0 == numread) {
				break;
			}
			for (j = 0; j < numread; j++) {
				if ('\n' == scanbuf[j])
					total++;
			}
		}

		lseek64(fd, 0, SEEK_SET);
		close(fd);
	}

	return total;
}


/*
 * Close the given file descriptor and open the next one, whose number in
 * the list is "filenum", returning the new file descriptor (or negative on
 * error). It is an error if the next input file is the same as the file
 * stdout is pointing to.
 *
 * Updates state->current_file in the process.
 */
int pv_next_file(pvstate_t state, int filenum, int oldfd)
{
	struct stat64 isb;
	struct stat64 osb;
	int fd, input_file_is_stdout;

	if (oldfd > 0) {
		if (close(oldfd)) {
			pv_error(state, "%s: %s",
				 _("failed to close file"),
				 strerror(errno));
			state->exit_status |= 8;
			return -1;
		}
	}

	if (filenum >= state->input_file_count) {
		state->exit_status |= 8;
		return -1;
	}

	if (filenum < 0) {
		state->exit_status |= 8;
		return -1;
	}

	if (0 == strcmp(state->input_files[filenum], "-")) {
		fd = STDIN_FILENO;
	} else {
		fd = open64(state->input_files[filenum], O_RDONLY);
		if (fd < 0) {
			pv_error(state, "%s: %s: %s",
				 _("failed to read file"),
				 state->input_files[filenum],
				 strerror(errno));
			state->exit_status |= 2;
			return -1;
		}
	}

	if (fstat64(fd, &isb)) {
		pv_error(state, "%s: %s: %s",
			 _("failed to stat file"),
			 state->input_files[filenum], strerror(errno));
		close(fd);
		state->exit_status |= 2;
		return -1;
	}

	if (fstat64(STDOUT_FILENO, &osb)) {
		pv_error(state, "%s: %s",
			 _("failed to stat output file"), strerror(errno));
		close(fd);
		state->exit_status |= 2;
		return -1;
	}

	/*
	 * Check that this new input file is not the same as stdout's
	 * destination. This restriction is ignored for anything other
	 * than a regular file or block device.
	 */
	input_file_is_stdout = 1;
	if (isb.st_dev != osb.st_dev)
		input_file_is_stdout = 0;
	if (isb.st_ino != osb.st_ino)
		input_file_is_stdout = 0;
	if (isatty(fd))
		input_file_is_stdout = 0;
	if ((!S_ISREG(isb.st_mode)) && (!S_ISBLK(isb.st_mode)))
		input_file_is_stdout = 0;

	if (input_file_is_stdout) {
		pv_error(state, "%s: %s",
			 _("input file is output file"),
			 state->input_files[filenum]);
		close(fd);
		state->exit_status |= 4;
		return -1;
	}

	state->current_file = state->input_files[filenum];
	if (0 == strcmp(state->input_files[filenum], "-")) {
		state->current_file = "(stdin)";
	}
	return fd;
}

/* EOF */
