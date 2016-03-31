/*
 * Global program option structure and the parsing function prototype.
 */

#ifndef _OPTIONS_H
#define _OPTIONS_H 1

#ifdef __cplusplus
extern "C" {
#endif

struct opts_s;
typedef struct opts_s *opts_t;

struct opts_s {           /* structure describing run-time options */
	char *program_name;            /* name the program is running as */
	unsigned char do_nothing;      /* exit-without-doing-anything flag */
	unsigned char progress;        /* progress bar flag */
	unsigned char timer;           /* timer flag */
	unsigned char eta;             /* ETA flag */
	unsigned char fineta;          /* absolute ETA flag */
	unsigned char rate;            /* rate counter flag */
	unsigned char average_rate;    /* average rate counter flag */
	unsigned char bytes;           /* bytes transferred flag */
	unsigned char bufpercent;      /* transfer buffer percentage flag */
	unsigned int lastwritten;      /* show N bytes last written */
	unsigned char force;           /* force-if-not-terminal flag */
	unsigned char cursor;          /* whether to use cursor positioning */
	unsigned char numeric;         /* numeric output only */
	unsigned char wait;            /* wait for transfer before display */
	unsigned char linemode;        /* count lines instead of bytes */
	unsigned char null;            /* lines are null-terminated */
	unsigned char no_op;           /* do nothing other than pipe data */
	unsigned long long rate_limit; /* rate limit, in bytes per second */
	unsigned long long buffer_size;/* buffer size, in bytes (0=default) */
	unsigned int remote;           /* PID of pv to update settings of */
	unsigned long long size;       /* total size of data */
	unsigned char no_splice;       /* flag set if never to use splice */
	unsigned char skip_errors;     /* skip read errors flag */
	unsigned char stop_at_size;    /* set if we stop at "size" bytes */
	double interval;               /* interval between updates */
	double delay_start;            /* delay before first display */
	unsigned int watch_pid;	       /* process to watch fds of */
	int watch_fd;		       /* fd to watch */
	unsigned int width;            /* screen width */
	unsigned int height;           /* screen height */
	char *name;                    /* process name, if any */
	char *format;                  /* output format, if any */
	char *pidfile;                 /* PID file, if any */
	int argc;                      /* number of non-option arguments */
	char **argv;                   /* array of non-option arguments */
};

extern opts_t opts_parse(int, char **);
extern void opts_free(opts_t);


#ifdef __cplusplus
}
#endif

#endif /* _OPTIONS_H */

/* EOF */
