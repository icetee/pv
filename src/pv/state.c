/*
 * State management functions.
 */

#include "pv-internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/*
 * Create a new state structure, and return it, or 0 (NULL) on error.
 */
pvstate_t pv_state_alloc(const char *program_name)
{
	pvstate_t state;

	state = calloc(1, sizeof(*state));
	if (0 == state)
		return 0;

	state->program_name = program_name;
	state->watch_pid = 0;
	state->watch_fd = -1;
#ifdef HAVE_IPC
	state->crs_shmid = -1;
	state->crs_pvcount = 1;
#endif				/* HAVE_IPC */
	state->crs_lock_fd = -1;

	state->reparse_display = 1;
	state->current_file = _("none");
#ifdef HAVE_SPLICE
	state->splice_failed_fd = -1;
#endif				/* HAVE_SPLICE */
	state->display_visible = 0;

	return state;
}


/*
 * Free a state structure, after which it can no longer be used.
 */
void pv_state_free(pvstate_t state)
{
	if (0 == state)
		return;

	if (state->display_buffer)
		free(state->display_buffer);
	state->display_buffer = NULL;

	if (state->transfer_buffer)
		free(state->transfer_buffer);
	state->transfer_buffer = NULL;

	free(state);

	return;
}


/*
 * Set the formatting string, given a set of old-style formatting options.
 */
void pv_state_set_format(pvstate_t state, unsigned char progress,
			 unsigned char timer, unsigned char eta,
			 unsigned char fineta, unsigned char rate,
			 unsigned char average_rate, unsigned char bytes,
			 unsigned char bufpercent,
			 unsigned int lastwritten, const char *name)
{
#define PV_ADDFORMAT(x,y) if (x) { \
		if (state->default_format[0] != 0) \
			strcat(state->default_format, " "); \
		strcat(state->default_format, y); \
	}

	state->default_format[0] = 0;
	PV_ADDFORMAT(name, "%N");
	PV_ADDFORMAT(bytes, "%b");
	PV_ADDFORMAT(bufpercent, "%T");
	PV_ADDFORMAT(timer, "%t");
	PV_ADDFORMAT(rate, "%r");
	PV_ADDFORMAT(average_rate, "%a");
	PV_ADDFORMAT(progress, "%p");
	PV_ADDFORMAT(eta, "%e");
	PV_ADDFORMAT(fineta, "%I");
	if (lastwritten > 0) {
		char buf[16];
		sprintf(buf, "%%%uA", lastwritten);
		PV_ADDFORMAT(lastwritten, buf);
	}

	state->name = name;
	state->reparse_display = 1;
}


void pv_state_force_set(pvstate_t state, unsigned char val)
{
	state->force = val;
}

void pv_state_cursor_set(pvstate_t state, unsigned char val)
{
	state->cursor = val;
};

void pv_state_numeric_set(pvstate_t state, unsigned char val)
{
	state->numeric = val;
};

void pv_state_wait_set(pvstate_t state, unsigned char val)
{
	state->wait = val;
};

void pv_state_delay_start_set(pvstate_t state, double val)
{
	state->delay_start = val;
};

void pv_state_linemode_set(pvstate_t state, unsigned char val)
{
	state->linemode = val;
};

void pv_state_null_set(pvstate_t state, unsigned char val)
{
	state->null = val;
};

void pv_state_no_op_set(pvstate_t state, unsigned char val)
{
	state->no_op = val;
};

void pv_state_skip_errors_set(pvstate_t state, unsigned char val)
{
	state->skip_errors = val;
};

void pv_state_stop_at_size_set(pvstate_t state, unsigned char val)
{
	state->stop_at_size = val;
};

void pv_state_rate_limit_set(pvstate_t state, unsigned long long val)
{
	state->rate_limit = val;
};

void pv_state_target_buffer_size_set(pvstate_t state,
				     unsigned long long val)
{
	state->target_buffer_size = val;
};

void pv_state_no_splice_set(pvstate_t state, unsigned char val)
{
	state->no_splice = val;
};

void pv_state_size_set(pvstate_t state, unsigned long long val)
{
	state->size = val;
};

void pv_state_interval_set(pvstate_t state, double val)
{
	state->interval = val;
};

void pv_state_width_set(pvstate_t state, unsigned int val)
{
	state->width = val;
};

void pv_state_height_set(pvstate_t state, unsigned int val)
{
	state->height = val;
};

void pv_state_name_set(pvstate_t state, const char *val)
{
	state->name = val;
};

void pv_state_format_string_set(pvstate_t state, const char *val)
{
	state->format_string = val;
};

void pv_state_watch_pid_set(pvstate_t state, unsigned int val)
{
	state->watch_pid = val;
};

void pv_state_watch_fd_set(pvstate_t state, int val)
{
	state->watch_fd = val;
};


/*
 * Set the array of input files.
 */
void pv_state_inputfiles(pvstate_t state, int input_file_count,
			 const char **input_files)
{
	state->input_file_count = input_file_count;
	state->input_files = input_files;
}

/* EOF */
