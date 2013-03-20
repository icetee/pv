/*
 * Parse command-line options.
 *
 * Copyright 2013 Andrew Wood, distributed under the Artistic License 2.0.
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
		{"rate", 0, 0, 'r'},
		{"average-rate", 0, 0, 'a'},
		{"bytes", 0, 0, 'b'},
		{"force", 0, 0, 'f'},
		{"numeric", 0, 0, 'n'},
		{"quiet", 0, 0, 'q'},
		{"cursor", 0, 0, 'c'},
		{"wait", 0, 0, 'W'},
		{"size", 1, 0, 's'},
		{"line-mode", 0, 0, 'l'},
		{"interval", 1, 0, 'i'},
		{"width", 1, 0, 'w'},
		{"height", 1, 0, 'H'},
		{"name", 1, 0, 'N'},
		{"format", 1, 0, 'F'},
		{"rate-limit", 1, 0, 'L'},
		{"buffer-size", 1, 0, 'B'},
		{"skip-errors", 0, 0, 'E'},
		{"stop-at-size", 0, 0, 'S'},
		{"remote", 1, 0, 'R'},
		{"pidfile", 1, 0, 'P'},
		{0, 0, 0, 0}
	};
	int option_index = 0;
#endif
	char *short_options = "hVpterabfnqcWs:li:w:H:N:F:L:B:ESR:P:";
	int c, numopts;
	opts_t opts;

	opts = calloc(1, sizeof(*opts));
	if (!opts) {
		fprintf(stderr,		    /* RATS: ignore */
			_("%s: option structure allocation failed (%s)"),
			argv[0], strerror(errno));
		fprintf(stderr, "\n");
		return 0;
	}

	opts->program_name = argv[0];

	opts->argc = 0;
	opts->argv = calloc(argc + 1, sizeof(char *));
	if (!opts->argv) {
		fprintf(stderr,		    /* RATS: ignore */
			_
			("%s: option structure argv allocation failed (%s)"),
			argv[0], strerror(errno));
		fprintf(stderr, "\n");
		opts_free(opts);
		return 0;
	}

	numopts = 0;

	opts->interval = 1;

	do {
#ifdef HAVE_GETOPT_LONG
		c = getopt_long(argc, argv, /* RATS: ignore */
				short_options, long_options,
				&option_index);
#else
		c = getopt(argc, argv, short_options);	/* RATS: ignore */
#endif
		if (c < 0)
			continue;

		/*
		 * Check that any numeric arguments are of the right type.
		 */
		switch (c) {
		case 's':
		case 'w':
		case 'H':
		case 'L':
		case 'B':
		case 'R':
			if (pv_getnum_check(optarg, PV_NUMTYPE_INTEGER) !=
			    0) {
				fprintf(stderr, "%s: -%c: %s\n", argv[0],
					c, _("integer argument expected"));
				opts_free(opts);
				return 0;
			}
			break;
		case 'i':
			if (pv_getnum_check(optarg, PV_NUMTYPE_DOUBLE) !=
			    0) {
				fprintf(stderr, "%s: -%c: %s\n", argv[0],
					c, _("numeric argument expected"));
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
		case 's':
			opts->size = pv_getnum_ll(optarg);
			break;
		case 'l':
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
		default:
#ifdef HAVE_GETOPT_LONG
			fprintf(stderr,	    /* RATS: ignore (OK) */
				_("Try `%s --help' for more information."),
				argv[0]);
#else
			fprintf(stderr,	    /* RATS: ignore (OK) */
				_("Try `%s -h' for more information."),
				argv[0]);
#endif
			fprintf(stderr, "\n");
			opts_free(opts);
			return 0;
			break;
		}

	} while (c != -1);

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
