/*
 * Parse command-line options.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "options.h"
#include "library/getopt.h"
#include "pv.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>


void display_help(void);
void display_version(void);


/*
 * Free an opts_t object.
 */
void opts_free(opts_t opts)
{
	if (!opts)
		return;
	if (opts->argv)
		free(opts->argv);
	free(opts);
}


/*
 * Parse the given command-line arguments into an opts_t object, handling
 * "help", "license" and "version" options internally.
 *
 * Returns an opts_t, or 0 on error.
 *
 * Note that the contents of *argv[] (i.e. the command line parameters)
 * aren't copied anywhere, just the pointers are copied, so make sure the
 * command line data isn't overwritten or argv[1] free()d or whatever.
 */
opts_t opts_parse(int argc, char **argv)
{
#ifdef HAVE_GETOPT_LONG
	struct option long_options[] = {
		{"help", 0, 0, 'h'},
		{"version", 0, 0, 'V'},
		{"progress", 0, 0, 'p'},
		{"timer", 0, 0, 't'},
		{"eta", 0, 0, 'e'},
		{"fineta", 0, 0, 'I'},
		{"rate", 0, 0, 'r'},
		{"average-rate", 0, 0, 'a'},
		{"bytes", 0, 0, 'b'},
		{"buffer-percent", 0, 0, 'T'},
		{"last-written", 1, 0, 'A'},
		{"force", 0, 0, 'f'},
		{"numeric", 0, 0, 'n'},
		{"quiet", 0, 0, 'q'},
		{"cursor", 0, 0, 'c'},
		{"wait", 0, 0, 'W'},
		{"delay-start", 1, 0, 'D'},
		{"size", 1, 0, 's'},
		{"line-mode", 0, 0, 'l'},
		{"null", 0, 0, '0'},
		{"interval", 1, 0, 'i'},
		{"width", 1, 0, 'w'},
		{"height", 1, 0, 'H'},
		{"name", 1, 0, 'N'},
		{"format", 1, 0, 'F'},
		{"rate-limit", 1, 0, 'L'},
		{"buffer-size", 1, 0, 'B'},
		{"no-splice", 0, 0, 'C'},
		{"skip-errors", 0, 0, 'E'},
		{"stop-at-size", 0, 0, 'S'},
		{"remote", 1, 0, 'R'},
		{"pidfile", 1, 0, 'P'},
		{"watchfd", 1, 0, 'd'},
		{0, 0, 0, 0}
	};
	int option_index = 0;
#endif
	char *short_options =
	    "hVpteIrabTA:fnqcWD:s:l0i:w:H:N:F:L:B:CESR:P:d:";
	int c, numopts;
	unsigned int check_pid;
	int check_fd;
	opts_t opts;
	char *ptr;

	opts = calloc(1, sizeof(*opts));
	if (!opts) {
		fprintf(stderr,
			_("%s: option structure allocation failed (%s)"),
			argv[0], strerror(errno));
		fprintf(stderr, "\n");
		return 0;
	}

	opts->program_name = argv[0];
	ptr = strrchr(opts->program_name, '/');
	if (NULL != ptr)
		opts->program_name = &(ptr[1]);

	opts->argc = 0;
	opts->argv = calloc(argc + 1, sizeof(char *));
	if (!opts->argv) {
		fprintf(stderr,
			_
			("%s: option structure argv allocation failed (%s)"),
			opts->program_name, strerror(errno));
		fprintf(stderr, "\n");
		opts_free(opts);
		return 0;
	}

	numopts = 0;

	opts->interval = 1;
	opts->delay_start = 0;
	opts->watch_pid = 0;
	opts->watch_fd = -1;

	do {
#ifdef HAVE_GETOPT_LONG
		c = getopt_long(argc, argv,
				short_options, long_options,
				&option_index);
#else
		c = getopt(argc, argv, short_options);
#endif
		if (c < 0)
			continue;

		/*
		 * Check that any numeric arguments are of the right type.
		 */
		switch (c) {
		case 's':
		case 'A':
		case 'w':
		case 'H':
		case 'L':
		case 'B':
		case 'R':
			if (pv_getnum_check(optarg, PV_NUMTYPE_INTEGER) !=
			    0) {
				fprintf(stderr, "%s: -%c: %s\n",
					opts->program_name, c,
					_("integer argument expected"));
				opts_free(opts);
				return 0;
			}
			break;
		case 'i':
		case 'D':
			if (pv_getnum_check(optarg, PV_NUMTYPE_DOUBLE) !=
			    0) {
				fprintf(stderr, "%s: -%c: %s\n",
					opts->program_name, c,
					_("numeric argument expected"));
				opts_free(opts);
				return 0;
			}
			break;
		case 'd':
			if (sscanf(optarg, "%u:%d", &check_pid, &check_fd)
			    < 1) {
				fprintf(stderr, "%s: -%c: %s\n",
					opts->program_name, c,
					_
					("process ID or pid:fd pair expected"));
				opts_free(opts);
				return 0;
			}
			if (check_pid < 1) {
				fprintf(stderr, "%s: -%c: %s\n",
					opts->program_name, c,
					_("invalid process ID"));
				opts_free(opts);
				return 0;
			}
			break;
		default:
			break;
		}

		/*
		 * Parse each command line option.
		 */
		switch (c) {
		case 'h':
			display_help();
			opts->do_nothing = 1;
			return opts;
			break;
		case 'V':
			display_version();
			opts->do_nothing = 1;
			return opts;
			break;
		case 'p':
			opts->progress = 1;
			numopts++;
			break;
		case 't':
			opts->timer = 1;
			numopts++;
			break;
		case 'I':
			opts->fineta = 1;
			numopts++;
			break;
		case 'e':
			opts->eta = 1;
			numopts++;
			break;
		case 'r':
			opts->rate = 1;
			numopts++;
			break;
		case 'a':
			opts->average_rate = 1;
			numopts++;
			break;
		case 'b':
			opts->bytes = 1;
			numopts++;
			break;
		case 'T':
			opts->bufpercent = 1;
			numopts++;
			break;
		case 'A':
			opts->lastwritten = pv_getnum_i(optarg);
			numopts++;
			break;
		case 'f':
			opts->force = 1;
			break;
		case 'n':
			opts->numeric = 1;
			numopts++;
			break;
		case 'q':
			opts->no_op = 1;
			numopts++;
			break;
		case 'c':
			opts->cursor = 1;
			break;
		case 'W':
			opts->wait = 1;
			break;
		case 'D':
			opts->delay_start = pv_getnum_d(optarg);
			break;
		case 's':
			opts->size = pv_getnum_ll(optarg);
			break;
		case 'l':
			opts->linemode = 1;
			break;
		case '0':
			opts->null = 1;
			opts->linemode = 1;
			break;
		case 'i':
			opts->interval = pv_getnum_d(optarg);
			break;
		case 'w':
			opts->width = pv_getnum_i(optarg);
			break;
		case 'H':
			opts->height = pv_getnum_i(optarg);
			break;
		case 'N':
			opts->name = optarg;
			break;
		case 'L':
			opts->rate_limit = pv_getnum_ll(optarg);
			break;
		case 'B':
			opts->buffer_size = pv_getnum_ll(optarg);
			break;
		case 'C':
			opts->no_splice = 1;
			break;
		case 'E':
			opts->skip_errors++;
			break;
		case 'S':
			opts->stop_at_size = 1;
			break;
		case 'R':
			opts->remote = pv_getnum_i(optarg);
			break;
		case 'P':
			opts->pidfile = optarg;
			break;
		case 'F':
			opts->format = optarg;
			break;
		case 'd':
			opts->watch_pid = 0;
			opts->watch_fd = -1;
			sscanf(optarg, "%u:%d", &(opts->watch_pid),
			       &(opts->watch_fd));
			break;
		default:
#ifdef HAVE_GETOPT_LONG
			fprintf(stderr,
				_("Try `%s --help' for more information."),
				opts->program_name);
#else
			fprintf(stderr,
				_("Try `%s -h' for more information."),
				opts->program_name);
#endif
			fprintf(stderr, "\n");
			opts_free(opts);
			return 0;
			break;
		}

	} while (c != -1);

	if (0 != opts->watch_pid) {
		if (opts->linemode || opts->null || opts->stop_at_size
		    || (opts->skip_errors > 0) || (opts->buffer_size > 0)
		    || (opts->rate_limit > 0)) {
			fprintf(stderr,
				_
				("%s: cannot use line mode or transfer modifier options when watching file descriptors"),
				opts->program_name);
			fprintf(stderr, "\n");
			opts_free(opts);
			return 0;
		}

		if (opts->cursor) {
			fprintf(stderr,
				_
				("%s: cannot use cursor positioning when watching file descriptors"),
				opts->program_name);
			fprintf(stderr, "\n");
			opts_free(opts);
			return 0;
		}

		if (0 != opts->remote) {
			fprintf(stderr,
				_
				("%s: cannot use remote control when watching file descriptors"),
				opts->program_name);
			fprintf(stderr, "\n");
			opts_free(opts);
			return 0;
		}

		if (optind < argc) {
			fprintf(stderr,
				_
				("%s: cannot transfer files when watching file descriptors"),
				opts->program_name);
			fprintf(stderr, "\n");
			opts_free(opts);
			return 0;
		}

		if (0 != access("/proc/self/fdinfo", X_OK)) {
			fprintf(stderr,
				_
				("%s: -d: not available on systems without /proc/self/fdinfo"),
				opts->program_name);
			fprintf(stderr, "\n");
			opts_free(opts);
			return 0;
		}
	}

	/*
	 * Default options: -pterb
	 */
	if (0 == numopts) {
		opts->progress = 1;
		opts->timer = 1;
		opts->eta = 1;
		opts->rate = 1;
		opts->bytes = 1;
	}

	/*
	 * Store remaining command-line arguments.
	 */
	while (optind < argc) {
		opts->argv[opts->argc++] = argv[optind++];
	}

	return opts;
}

/* EOF */
