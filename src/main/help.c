/*
 * Output command-line help to stdout.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#define N_(String) (String)

struct optdesc_s {
	char *optshort;
	char *optlong;
	char *param;
	char *description;
};


/*
 * Display command-line help.
 */
void display_help(void)
{
	struct optdesc_s optlist[] = {
		{"-p", "--progress", 0,
		 N_("show progress bar")},
		{"-t", "--timer", 0,
		 N_("show elapsed time")},
		{"-e", "--eta", 0,
		 N_("show estimated time of arrival (completion)")},
		{"-I", "--fineta", 0,
		 N_
		 ("show absolute estimated time of arrival (completion)")},
		{"-r", "--rate", 0,
		 N_("show data transfer rate counter")},
		{"-a", "--average-rate", 0,
		 N_("show data transfer average rate counter")},
		{"-b", "--bytes", 0,
		 N_("show number of bytes transferred")},
		{"-T", "--buffer-percent", 0,
		 N_("show percentage of transfer buffer in use")},
		{"-A", "--last-written", _("NUM"),
		 N_("show NUM bytes last written")},
		{"-F", "--format", N_("FORMAT"),
		 N_("set output format to FORMAT")},
		{"-n", "--numeric", 0,
		 N_("output percentages, not visual information")},
		{"-q", "--quiet", 0,
		 N_("do not output any transfer information at all")},
		{"", 0, 0, 0},
		{"-W", "--wait", 0,
		 N_("display nothing until first byte transferred")},
		{"-D", "--delay-start", N_("SEC"),
		 N_("display nothing until SEC seconds have passed")},
		{"-s", "--size", N_("SIZE"),
		 N_("set estimated data size to SIZE bytes")},
		{"-l", "--line-mode", 0,
		 N_("count lines instead of bytes")},
		{"-0", "--null", 0,
		 N_("lines are null-terminated")},
		{"-i", "--interval", N_("SEC"),
		 N_("update every SEC seconds")},
		{"-w", "--width", N_("WIDTH"),
		 N_("assume terminal is WIDTH characters wide")},
		{"-H", "--height", N_("HEIGHT"),
		 N_("assume terminal is HEIGHT rows high")},
		{"-N", "--name", N_("NAME"),
		 N_("prefix visual information with NAME")},
		{"-f", "--force", 0,
		 N_("output even if standard error is not a terminal")},
		{"-c", "--cursor", 0,
		 N_("use cursor positioning escape sequences")},
		{"", 0, 0, 0},
		{"-L", "--rate-limit", N_("RATE"),
		 N_("limit transfer to RATE bytes per second")},
		{"-B", "--buffer-size", N_("BYTES"),
		 N_("use a buffer size of BYTES")},
		{"-C", "--no-splice", 0,
		 N_("never use splice(), always use read/write")},
		{"-E", "--skip-errors", 0,
		 N_("skip read errors in input")},
		{"-S", "--stop-at-size", 0,
		 N_("stop after --size bytes have been transferred")},
#ifdef HAVE_IPC
		{"-R", "--remote", N_("PID"),
		 N_("update settings of process PID")},
#endif				/* HAVE_IPC */
		{"", 0, 0, 0},
		{"-P", "--pidfile", N_("FILE"),
		 N_("save process ID in FILE")},
		{"", 0, 0, 0},
		{"-d", "--watchfd", N_("PID[:FD]"),
		 N_("watch file FD opened by process PID")},
		{"", 0, 0, 0},
		{"-h", "--help", 0,
		 N_("show this help and exit")},
		{"-V", "--version", 0,
		 N_("show version information and exit")},
		{0, 0, 0, 0}
	};
	int i, col1max = 0, tw = 77;
	char *optbuf;

	printf(_("Usage: %s [OPTION] [FILE]..."), PROGRAM_NAME);
	printf("\n%s\n\n",
	       _
	       ("Concatenate FILE(s), or standard input, to standard output,\n"
		"with monitoring."));

	for (i = 0; optlist[i].optshort; i++) {
		int width = 0;
		char *param;

		width = 2 + strlen(optlist[i].optshort);
#ifdef HAVE_GETOPT_LONG
		if (optlist[i].optlong)
			width += 2 + strlen(optlist[i].optlong);
#endif
		param = optlist[i].param;
		if (param)
			param = _(param);
		if (param)
			width += 1 + strlen(param);

		if (width > col1max)
			col1max = width;
	}

	col1max++;

	optbuf = malloc(col1max + 16);
	if (NULL == optbuf) {
		fprintf(stderr, "%s: %s\n", PROGRAM_NAME, strerror(errno));
		exit(1);
	}

	for (i = 0; optlist[i].optshort; i++) {
		char *param;
		char *description;
		char *start;
		char *end;

		if (0 == optlist[i].optshort[0]) {
			printf("\n");
			continue;
		}

		param = optlist[i].param;
		if (param)
			param = _(param);
		description = optlist[i].description;
		if (description)
			description = _(description);

		sprintf(optbuf, "%s%s%s%s%s", optlist[i].optshort,
#ifdef HAVE_GETOPT_LONG
			optlist[i].optlong ? ", " : "",
			optlist[i].optlong ? optlist[i].optlong : "",
#else
			"", "",
#endif
			param ? " " : "", param ? param : "");

		printf("  %-*s ", col1max - 2, optbuf);

		if (NULL == description) {
			printf("\n");
			continue;
		}

		start = description;

		while (strlen(start) > tw - col1max) {
			end = start + tw - col1max;
			while ((end > start) && (end[0] != ' '))
				end--;
			if (end == start) {
				end = start + tw - col1max;
			} else {
				end++;
			}
			printf("%.*s\n%*s ", (int) (end - start), start,
			       col1max, "");
			if (end == start)
				end++;
			start = end;
		}

		printf("%s\n", start);
	}

#ifdef ENABLE_DEBUGGING
	printf("\n");
	printf("%s",
	       _
	       ("Debugging is enabled; export the DEBUG environment variable to define the\noutput filename.\n"));
#endif

	printf("\n");
	printf(_("Please report any bugs to %s."), BUG_REPORTS_TO);
	printf("\n");
}

/* EOF */
