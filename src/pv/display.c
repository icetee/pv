/*
 * Display functions.
 */

#include "pv-internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>


/*
 * Output an error message.  If we've displayed anything to the terminal
 * already, then put a newline before our error so we don't write over what
 * we've written.
 */
void pv_error(pvstate_t state, char *format, ...)
{
	va_list ap;
	if (state->display_visible)
		fprintf(stderr, "\n");
	fprintf(stderr, "%s: ", state->program_name);
	va_start(ap, format);
	vfprintf(stderr, format, ap);
	va_end(ap);
	fprintf(stderr, "\n");
}


/*
 * Fill in *width and *height with the current terminal size,
 * if possible.
 */
void pv_screensize(unsigned int *width, unsigned int *height)
{
#ifdef TIOCGWINSZ
	struct winsize wsz;

	if (isatty(STDERR_FILENO)) {
		if (0 == ioctl(STDERR_FILENO, TIOCGWINSZ, &wsz)) {
			*width = wsz.ws_col;
			*height = wsz.ws_row;
		}
	}
#endif
}


/*
 * Calculate the percentage transferred so far and return it.
 */
static long pv__calc_percentage(long long so_far, const long long total)
{
	if (total < 1)
		return 0;

	so_far *= 100;
	so_far /= total;

	return (long) so_far;
}


/*
 * Given how many bytes have been transferred, the total byte count to
 * transfer, and how long it's taken so far in seconds, return the estimated
 * number of seconds until completion.
 */
static long pv__calc_eta(const long long so_far, const long long total,
			 const long elapsed)
{
	long long amount_left;

	if (so_far < 1)
		return 0;

	amount_left = total - so_far;
	amount_left *= (long long) elapsed;
	amount_left /= so_far;

	return (long) amount_left;
}

/*
 * Given a long double value, it is divided or multiplied by the ratio until
 * a value in the range 1.0 to 999.999... is found.  The string "prefix" to
 * is updated to the corresponding SI prefix.
 *
 * If "is_bytes" is 1, then the second byte of "prefix" is set to "i" to
 * denote MiB etc (IEEE1541).  Thus "prefix" should be at least 3 bytes long
 * (to include the terminating null).
 *
 * Submitted by Henry Gebhardt <hsggebhardt@googlemail.com> and then
 * modified.  Further changed after input from Thomas Rachel; changed still
 * further after Debian bug #706175.
 */
static void pv__si_prefix(long double *value, char *prefix,
			  const long double ratio, int is_bytes)
{
	static char *pfx_000 = NULL;	 /* kilo, mega, etc */
	static char *pfx_024 = NULL;	 /* kibi, mibi, etc */
	static char const *pfx_middle_000 = NULL;
	static char const *pfx_middle_024 = NULL;
	char *pfx;
	char const *pfx_middle;
	char const *i;
	long double cutoff;

	if (NULL == pfx_000) {
		pfx_000 = _("yzafpnum kMGTPEZY");
		pfx_middle_000 = strchr(pfx_000, ' ');
	}

	if (NULL == pfx_024) {
		pfx_024 = _("yzafpnum KMGTPEZY");
		pfx_middle_024 = strchr(pfx_024, ' ');
	}

	pfx = pfx_000;
	pfx_middle = pfx_middle_000;
	if (is_bytes) {
		pfx = pfx_024;
		pfx_middle = pfx_middle_024;
	}

	i = pfx_middle;

	prefix[0] = ' ';		    /* Make the prefix start blank. */
	prefix[1] = 0;

	/*
	 * Force an empty prefix if the value is zero to avoid "0yB".
	 */
	if (0.0 == *value)
		return;

	cutoff = ratio * 0.97;

	while ((*value > cutoff) && (*(i += 1) != '\0')) {
		*value /= ratio;
		prefix[0] = *i;
	}

	while ((*value < 1.0) && ((i -= 1) != (pfx - 1))) {
		*value *= ratio;
		prefix[0] = *i;
	}

	if (is_bytes && prefix[0] != ' ') {
		prefix[1] = 'i';
		prefix[2] = 0;
	}
}


/*
 * Put a string in "buffer" (max length "bufsize") containing "amount"
 * formatted such that it's 3 or 4 digits followed by an SI suffix and then
 * whichever of "suffix_basic" or "suffix_bytes" is appropriate (whether
 * "is_bytes" is 0 for non-byte amounts or 1 for byte amounts). If
 * "is_bytes" is 1 then the SI units are KiB, MiB etc and the divisor is
 * 1024 instead of 1000.
 *
 * The "format" string is in sprintf format and must contain exactly one %
 * parameter (a %s) which will expand to the string described above.
 */
static void pv__sizestr(char *buffer, int bufsize, char *format,
			long double amount, char *suffix_basic,
			char *suffix_bytes, int is_bytes)
{
	char sizestr_buffer[256];
	char si_prefix[8] = " ";
	long double divider;
	long double display_amount;
	char *suffix;

	if (is_bytes) {
		suffix = suffix_bytes;
		divider = 1024.0;
	} else {
		suffix = suffix_basic;
		divider = 1000.0;
	}

	display_amount = amount;

	pv__si_prefix(&display_amount, si_prefix, divider, is_bytes);

	/* Make sure we don't overrun our buffer. */
	if (display_amount > 100000)
		display_amount = 100000;

	/* Fix for display of "1.01e+03" instead of "1010" */
	if (display_amount > 99.9) {
		sprintf(sizestr_buffer, "%4ld%.2s%.16s",
			(long) display_amount, si_prefix, suffix);
	} else {
		/*
		 * AIX blows up with %4.3Lg%.2s%.16s for some reason, so we
		 * write display_amount separately first.
		 */
		char str_disp[64];
		sprintf(str_disp, "%4.3Lg", display_amount);
		sprintf(sizestr_buffer, "%s%.2s%.16s",
			str_disp, si_prefix, suffix);
	}

	snprintf(buffer, bufsize, format, sizestr_buffer);
}


/*
 * Initialise the output format structure, based on the current options.
 */
static void pv__format_init(pvstate_t state)
{
	const char *formatstr;
	const char *searchptr;
	int strpos;
	int segment;

	if (NULL == state)
		return;

	state->str_name[0] = 0;
	state->str_transferred[0] = 0;
	state->str_timer[0] = 0;
	state->str_rate[0] = 0;
	state->str_average_rate[0] = 0;
	state->str_progress[0] = 0;
	state->str_eta[0] = 0;
	memset(state->format, 0, sizeof(state->format));

	if (state->name) {
		sprintf(state->str_name, "%9.500s:", state->name);
	}

	formatstr =
	    state->format_string ? state->format_string : state->
	    default_format;

	state->components_used = 0;

	/*
	 * Split the format string into segments.  Each segment consists
	 * of a string pointer and a length.
	 *
	 * A length of zero indicates that the segment is a fixed-size
	 * component updated by pv__format(), so it is a pointer to one
	 * of the state->str_* buffers that pv__format() updates.
	 *
	 * A length below zero indicates that the segment is a variable
	 * sized component which will be recalculated by pv__format()
	 * after the length of all fixed-size segments is known, and so
	 * the string is a pointer to another state->str_* buffer
	 * (currently it will only ever be state->str_progress).
	 *
	 * A length above zero indicates that the segment is a constant
	 * string of the given length (not necessarily null terminated).
	 *
	 * In pv__format(), after the state->str_* buffers have all been
	 * filled in, the output string is generated by sticking all of
	 * these segments together.
	 */
	segment = 0;
	for (strpos = 0; formatstr[strpos] != 0 && segment < 99;
	     strpos++, segment++) {
		if ('%' == formatstr[strpos]) {
			int num;
			strpos++;
			num = 0;
			while (isdigit(formatstr[strpos])) {
				num = num * 10;
				num += formatstr[strpos] - '0';
				strpos++;
			}
			switch (formatstr[strpos]) {
			case 'p':
				state->format[segment].string =
				    state->str_progress;
				state->format[segment].length = -1;
				state->components_used |=
				    PV_DISPLAY_PROGRESS;
				break;
			case 't':
				state->format[segment].string =
				    state->str_timer;
				state->format[segment].length = 0;
				state->components_used |= PV_DISPLAY_TIMER;
				break;
			case 'e':
				state->format[segment].string =
				    state->str_eta;
				state->format[segment].length = 0;
				state->components_used |= PV_DISPLAY_ETA;
				break;
			case 'I':
				state->format[segment].string =
				    state->str_fineta;
				state->format[segment].length = 0;
				state->components_used |=
				    PV_DISPLAY_FINETA;
				break;
			case 'A':
				state->format[segment].string =
				    state->str_lastoutput;
				state->format[segment].length = 0;
				if (num > sizeof(state->lastoutput_buffer))
					num =
					    sizeof
					    (state->lastoutput_buffer);
				if (num < 1)
					num = 1;
				state->lastoutput_length = num;
				state->components_used |=
				    PV_DISPLAY_OUTPUTBUF;
				break;
			case 'r':
				state->format[segment].string =
				    state->str_rate;
				state->format[segment].length = 0;
				state->components_used |= PV_DISPLAY_RATE;
				break;
			case 'a':
				state->format[segment].string =
				    state->str_average_rate;
				state->format[segment].length = 0;
				state->components_used |=
				    PV_DISPLAY_AVERAGERATE;
				break;
			case 'b':
				state->format[segment].string =
				    state->str_transferred;
				state->format[segment].length = 0;
				state->components_used |= PV_DISPLAY_BYTES;
				break;
			case 'T':
				state->format[segment].string =
				    state->str_bufpercent;
				state->format[segment].length = 0;
				state->components_used |=
				    PV_DISPLAY_BUFPERCENT;
				break;
			case 'N':
				state->format[segment].string =
				    state->str_name;
				state->format[segment].length =
				    strlen(state->str_name);
				state->components_used |= PV_DISPLAY_NAME;
				break;
			case '%':
				/* %% => % */
				state->format[segment].string =
				    &(formatstr[strpos]);
				state->format[segment].length = 1;
				break;
			case 0:
				/* % at end => just % */
				state->format[segment].string =
				    &(formatstr[--strpos]);
				state->format[segment].length = 1;
				break;
			default:
				/* %z (unknown) => %z */
				state->format[segment].string =
				    &(formatstr[--strpos]);
				state->format[segment].length = 2;
				strpos++;
				break;
			}
		} else {
			int foundlength;
			searchptr = strchr(&(formatstr[strpos]), '%');
			if (NULL == searchptr) {
				foundlength = strlen(&(formatstr[strpos]));
			} else {
				foundlength =
				    searchptr - &(formatstr[strpos]);
			}
			state->format[segment].string =
			    &(formatstr[strpos]);
			state->format[segment].length = foundlength;
			strpos += foundlength - 1;
		}
	}

	state->format[segment].string = 0;
	state->format[segment].length = 0;
}

/*
 * Return the original value x so that it has been clamped between
 * [min..max]
 */
static long bound_long(long x, long min, long max)
{
	return x < min ? min : x > max ? max : x;
}


/*
 * Return a pointer to a string (which must not be freed), containing status
 * information formatted according to the state held within the given
 * structure, where "elapsed_sec" is the seconds elapsed since the transfer
 * started, "bytes_since_last" is the number of bytes transferred since the
 * last update, and "total_bytes" is the total number of bytes transferred
 * so far.
 *
 * If "bytes_since_last" is negative, this is the final update so the rate
 * is given as an an average over the whole transfer; otherwise the current
 * rate is shown.
 *
 * In line mode, "bytes_since_last" and "total_bytes" are in lines, not bytes.
 *
 * If "total_bytes" is negative, then free all allocated memory and return
 * NULL.
 */
static char *pv__format(pvstate_t state,
			long double elapsed_sec,
			long long bytes_since_last, long long total_bytes)
{
	long double time_since_last, rate, average_rate;
	long eta;
	int static_portion_size;
	int segment;
	int output_length;
	int display_string_length;

	/* Quick sanity check - state must exist */
	if (NULL == state)
		return NULL;

	/* Negative total transfer - free memory and exit */
	if (total_bytes < 0) {
		if (state->display_buffer)
			free(state->display_buffer);
		state->display_buffer = NULL;
		return NULL;
	}

	/*
	 * In case the time since the last update is very small, we keep
	 * track of amount transferred since the last update, and just keep
	 * adding to that until a reasonable amount of time has passed to
	 * avoid rate spikes or division by zero.
	 */
	time_since_last = elapsed_sec - state->prev_elapsed_sec;
	if (time_since_last <= 0.01) {
		rate = state->prev_rate;
		state->prev_trans += bytes_since_last;
	} else {
		rate =
		    ((long double) bytes_since_last +
		     state->prev_trans) / time_since_last;
		state->prev_elapsed_sec = elapsed_sec;
		state->prev_trans = 0;
	}
	state->prev_rate = rate;

	/*
	 * We only calculate the overall average rate if this is the last
	 * update or if the average rate display is enabled. Otherwise it's
	 * not worth the extra CPU cycles.
	 */
	if ((bytes_since_last < 0)
	    || ((state->components_used & PV_DISPLAY_AVERAGERATE) != 0)) {
		/* Sanity check to avoid division by zero */
		if (elapsed_sec < 0.000001)
			elapsed_sec = 0.000001;
		average_rate =
		    (((long double) total_bytes) -
		     ((long double) state->initial_offset)) /
		    (long double) elapsed_sec;
		if (bytes_since_last < 0)
			rate = average_rate;
	}

	if (state->size <= 0) {
		/*
		 * If we don't know the total size of the incoming data,
		 * then for a percentage, we gradually increase the
		 * percentage completion as data arrives, to a maximum of
		 * 200, then reset it - we use this if we can't calculate
		 * it, so that the numeric percentage output will go
		 * 0%-100%, 100%-0%, 0%-100%, and so on.
		 */
		if (rate > 0)
			state->percentage += 2;
		if (state->percentage > 199)
			state->percentage = 0;
	} else if (state->numeric
		   || ((state->components_used & PV_DISPLAY_PROGRESS) !=
		       0)) {
		/*
		 * If we do know the total size, and we're going to show
		 * the percentage (numeric mode or a progress bar),
		 * calculate the percentage completion.
		 */
		state->percentage =
		    pv__calc_percentage(total_bytes, state->size);
	}

	/*
	 * Reallocate output buffer if width changes.
	 */
	if (state->display_buffer != NULL
	    && state->display_buffer_size < (state->width * 2)) {
		free(state->display_buffer);
		state->display_buffer = NULL;
		state->display_buffer_size = 0;
	}

	/*
	 * Allocate output buffer if there isn't one.
	 */
	if (NULL == state->display_buffer) {
		state->display_buffer_size = (2 * state->width) + 80;
		if (state->name)
			state->display_buffer_size += strlen(state->name);
		state->display_buffer =
		    malloc(state->display_buffer_size + 16);
		if (NULL == state->display_buffer) {
			pv_error(state, "%s: %s",
				 _("buffer allocation failed"),
				 strerror(errno));
			state->exit_status |= 64;
			return NULL;
		}
		state->display_buffer[0] = 0;
	}

	/*
	 * In numeric output mode, our output is just a number.
	 *
	 * Patch from Sami Liedes:
	 * With --timer we prefix the output with the elapsed time.
	 * With --bytes we output the bytes transferred so far instead
	 * of the percentage. (Or lines, if --lines was given with --bytes).
	 */
	if (state->numeric) {
		char numericprefix[128];

		numericprefix[0] = 0;

		if ((state->components_used & PV_DISPLAY_TIMER) != 0)
			sprintf(numericprefix, "%.4Lf ", elapsed_sec);

		if ((state->components_used & PV_DISPLAY_BYTES) != 0) {
			sprintf(state->display_buffer, "%.99s%lld\n",
				numericprefix, total_bytes);
		} else if (state->percentage > 100) {
			/* As mentioned above, we go 0-100, then 100-0. */
			sprintf(state->display_buffer, "%.99s%ld\n",
				numericprefix, 200 - state->percentage);
		} else {
			sprintf(state->display_buffer, "%.99s%ld\n",
				numericprefix, state->percentage);
		}

		return state->display_buffer;
	}

	/*
	 * First, work out what components we will be putting in the output
	 * buffer, and for those that don't depend on the total width
	 * available (i.e. all but the progress bar), prepare their strings
	 * to be placed in the output buffer.
	 */

	state->str_transferred[0] = 0;
	state->str_bufpercent[0] = 0;
	state->str_timer[0] = 0;
	state->str_rate[0] = 0;
	state->str_average_rate[0] = 0;
	state->str_progress[0] = 0;
	state->str_lastoutput[0] = 0;
	state->str_eta[0] = 0;
	state->str_fineta[0] = 0;

	/* If we're showing bytes transferred, set up the display string. */
	if ((state->components_used & PV_DISPLAY_BYTES) != 0) {
		pv__sizestr(state->str_transferred,
			    sizeof(state->str_transferred), "%s",
			    (long double) total_bytes, "", _("B"),
			    state->linemode ? 0 : 1);
	}

	/* Transfer buffer percentage - set up the display string. */
	if ((state->components_used & PV_DISPLAY_BUFPERCENT) != 0) {
		if (state->buffer_size > 0)
			sprintf(state->str_bufpercent, "{%3ld%%}",
				pv__calc_percentage(state->read_position -
						    state->write_position,
						    state->buffer_size));
#ifdef HAVE_SPLICE
		if (state->splice_used)
			strcpy(state->str_bufpercent, "{----}");
#endif
	}

	/* Timer - set up the display string. */
	if ((state->components_used & PV_DISPLAY_TIMER) != 0) {
		/*
		 * Bounds check, so we don't overrun the prefix buffer. This
		 * does mean that the timer will stop at a 100,000 hours,
		 * but since that's 11 years, it shouldn't be a problem.
		 */
		if (elapsed_sec > (long double) 360000000.0L)
			elapsed_sec = (long double) 360000000.0L;

		sprintf(state->str_timer, "%ld:%02ld:%02ld",
			((long) elapsed_sec) / 3600,
			(((long) elapsed_sec) / 60) % 60,
			((long) elapsed_sec) % 60);
	}

	/* Rate - set up the display string. */
	if ((state->components_used & PV_DISPLAY_RATE) != 0) {
		pv__sizestr(state->str_rate, sizeof(state->str_rate),
			    "[%s]", rate, _("/s"), _("B/s"),
			    state->linemode ? 0 : 1);
	}

	/* Average rate - set up the display string. */
	if ((state->components_used & PV_DISPLAY_AVERAGERATE) != 0) {
		pv__sizestr(state->str_average_rate,
			    sizeof(state->str_average_rate), "[%s]",
			    average_rate, _("/s"), _("B/s"),
			    state->linemode ? 0 : 1);
	}

	/* Last output bytes - set up the display string. */
	if ((state->components_used & PV_DISPLAY_OUTPUTBUF) != 0) {
		int idx;
		for (idx = 0; idx < state->lastoutput_length; idx++) {
			int c;
			c = state->lastoutput_buffer[idx];
			state->str_lastoutput[idx] = isprint(c) ? c : '.';
		}
		state->str_lastoutput[idx] = 0;
	}

	/* ETA (only if size is known) - set up the display string. */
	if (((state->components_used & PV_DISPLAY_ETA) != 0)
	    && (state->size > 0)) {
		eta =
		    pv__calc_eta(total_bytes - state->initial_offset,
				 state->size - state->initial_offset,
				 elapsed_sec);

		/*
		 * Bounds check, so we don't overrun the suffix buffer. This
		 * means the ETA will always be less than 100,000 hours.
		 */
		eta = bound_long(eta, 0, (long) 360000000L);

		/*
		 * If the ETA is more than a day, include a day count as
		 * well as hours, minutes, and seconds.
		 */
		if (eta > 86400L) {
			sprintf(state->str_eta,
				"%.16s %ld:%02ld:%02ld:%02ld", _("ETA"),
				eta / 86400, (eta / 3600) % 24,
				(eta / 60) % 60, eta % 60);
		} else {
			sprintf(state->str_eta, "%.16s %ld:%02ld:%02ld",
				_("ETA"), eta / 3600, (eta / 60) % 60,
				eta % 60);
		}

		/*
		 * If this is the final update, show a blank space where the
		 * ETA used to be.
		 */
		if (bytes_since_last < 0) {
			int i;
			for (i = 0; i < sizeof(state->str_eta)
			     && state->str_eta[i] != 0; i++) {
				state->str_eta[i] = ' ';
			}
		}
	}

	/* ETA as clock time (as above) - set up the display string. */
	if (((state->components_used & PV_DISPLAY_FINETA) != 0)
	    && (state->size > 0)) {
		/*
		 * The ETA may be hidden by a failed ETA string
		 * generation.
		 */
		int show_eta = 1;
		time_t now = time(NULL);
		time_t then;
		struct tm *time_ptr;
		char *time_format = NULL;

		eta =
		    pv__calc_eta(total_bytes - state->initial_offset,
				 state->size - state->initial_offset,
				 elapsed_sec);

		/*
		 * Bounds check, so we don't overrun the suffix buffer. This
		 * means the ETA will always be less than 100,000 hours.
		 */
		eta = bound_long(eta, 0, (long) 360000000L);

		/*
		 * Only include the date if the ETA is more than 6 hours
		 * away.
		 */
		if (eta > (long) (6 * 3600)) {
			time_format = "%Y-%m-%d %H:%M:%S";
		} else {
			time_format = "%H:%M:%S";
		}

		then = now + eta;
		time_ptr = localtime(&then);

		if (NULL == time_ptr) {
			show_eta = 0;
		} else {
			/* Localtime keeps data stored in a
			 * static buffer that gets overwritten
			 * by time functions. */
			struct tm time = *time_ptr;

			sprintf(state->str_fineta, "%.16s ", _("ETA"));
			strftime(state->str_fineta +
				 strlen(state->str_fineta),
				 sizeof(state->str_fineta) - 1 -
				 strlen(state->str_fineta),
				 time_format, &time);
		}

		if (!show_eta) {
			int i;
			for (i = 0; i < sizeof(state->str_fineta)
			     && state->str_fineta[i] != 0; i++) {
				state->str_fineta[i] = ' ';
			}
		}
	}

	/*
	 * Now go through all the static portions of the format to work
	 * out how much space will be left for any dynamic portions
	 * (i.e. the progress bar).
	 */
	static_portion_size = 0;
	for (segment = 0; state->format[segment].string; segment++) {
		if (state->format[segment].length < 0) {
			continue;
		} else if (state->format[segment].length > 0) {
			static_portion_size +=
			    state->format[segment].length;
		} else {
			static_portion_size +=
			    strlen(state->format[segment].string);
		}
	}

	debug("static_portion_size: %d", static_portion_size);

	/*
	 * Assemble the progress bar now we know how big it should be.
	 */
	if ((state->components_used & PV_DISPLAY_PROGRESS) != 0) {
		char pct[16];
		int available_width, i;

		strcpy(state->str_progress, "[");

		if (state->size > 0) {
			if (state->percentage < 0)
				state->percentage = 0;
			if (state->percentage > 100000)
				state->percentage = 100000;
			sprintf(pct, "%2ld%%", state->percentage);

			available_width =
			    state->width - static_portion_size -
			    strlen(pct) - 3;

			if (available_width < 0)
				available_width = 0;

			if (available_width >
			    sizeof(state->str_progress) - 16)
				available_width =
				    sizeof(state->str_progress) - 16;

			for (i = 0;
			     i <
			     (available_width * state->percentage) / 100 -
			     1; i++) {
				if (i < available_width)
					strcat(state->str_progress, "=");
			}
			if (i < available_width) {
				strcat(state->str_progress, ">");
				i++;
			}
			for (; i < available_width; i++) {
				strcat(state->str_progress, " ");
			}
			strcat(state->str_progress, "] ");
			strcat(state->str_progress, pct);
		} else {
			int p = state->percentage;

			available_width =
			    state->width - static_portion_size - 5;

			if (available_width < 0)
				available_width = 0;

			if (available_width >
			    sizeof(state->str_progress) - 16)
				available_width =
				    sizeof(state->str_progress) - 16;

			debug("available_width: %d", available_width);

			if (p > 100)
				p = 200 - p;
			for (i = 0; i < (available_width * p) / 100; i++) {
				if (i < available_width)
					strcat(state->str_progress, " ");
			}
			strcat(state->str_progress, "<=>");
			for (; i < available_width; i++) {
				strcat(state->str_progress, " ");
			}
			strcat(state->str_progress, "]");
		}

		/*
		 * If the progress bar won't fit, drop it.
		 */
		if (strlen(state->str_progress) + static_portion_size >
		    state->width)
			state->str_progress[0] = 0;
	}

	/*
	 * We can now build the output string using the format structure.
	 */

	state->display_buffer[0] = 0;
	display_string_length = 0;
	for (segment = 0; state->format[segment].string; segment++) {
		int segment_length;
		if (state->format[segment].length > 0) {
			segment_length = state->format[segment].length;
		} else {
			segment_length =
			    strlen(state->format[segment].string);
		}
		/* Skip empty segments */
		if (segment_length == 0)
			continue;
		/*
		 * Truncate segment if it would make the display string
		 * overflow the buffer
		 */
		if (segment_length + display_string_length >
		    state->display_buffer_size - 2)
			segment_length =
			    state->display_buffer_size -
			    display_string_length - 2;
		if (segment_length < 1)
			break;
		/* Skip segment if it would make the display too wide */
		if (segment_length + display_string_length > state->width)
			break;
		strncat(state->display_buffer,
			state->format[segment].string, segment_length);
		display_string_length += segment_length;
	}

	/*
	 * If the size of our output shrinks, we need to keep appending
	 * spaces at the end, so that we don't leave dangling bits behind.
	 */
	output_length = strlen(state->display_buffer);
	if ((output_length < state->prev_length)
	    && (state->width >= state->prev_width)) {
		char spaces[32];
		int spaces_to_add;
		spaces_to_add = state->prev_length - output_length;
		/* Upper boundary on number of spaces */
		if (spaces_to_add > 15) {
			spaces_to_add = 15;
		}
		output_length += spaces_to_add;
		spaces[spaces_to_add] = 0;
		while (--spaces_to_add >= 0) {
			spaces[spaces_to_add] = ' ';
		}
		strcat(state->display_buffer, spaces);
	}
	state->prev_width = state->width;
	state->prev_length = output_length;

	return state->display_buffer;
}


/*
 * Output status information on standard error, where "esec" is the seconds
 * elapsed since the transfer started, "sl" is the number of bytes transferred
 * since the last update, and "tot" is the total number of bytes transferred
 * so far.
 *
 * If "sl" is negative, this is the final update so the rate is given as an
 * an average over the whole transfer; otherwise the current rate is shown.
 *
 * In line mode, "sl" and "tot" are in lines, not bytes.
 */
void pv_display(pvstate_t state, long double esec, long long sl,
		long long tot)
{
	char *display;

	if (NULL == state)
		return;

	/*
	 * If the display options need reparsing, do so to generate new
	 * formatting parameters.
	 */
	if (state->reparse_display) {
		pv__format_init(state);
		state->reparse_display = 0;
	}

	pv_sig_checkbg();

	display = pv__format(state, esec, sl, tot);
	if (NULL == display)
		return;

	if (state->numeric) {
		write(STDERR_FILENO, display, strlen(display));
	} else if (state->cursor) {
		pv_crs_update(state, display);
		state->display_visible = 1;
	} else {
		write(STDERR_FILENO, display, strlen(display));
		write(STDERR_FILENO, "\r", 1);
		state->display_visible = 1;
	}

	debug("%s: [%s]", "display", display);
}

/* EOF */
