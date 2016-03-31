/*
 * Remote-control functions.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "options.h"
#include "pv.h"

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/time.h>

#ifdef HAVE_IPC
#include <sys/ipc.h>
#include <sys/msg.h>
#endif				/* HAVE_IPC */


#ifdef HAVE_IPC
struct remote_msg {
	long mtype;
	unsigned char progress;		 /* progress bar flag */
	unsigned char timer;		 /* timer flag */
	unsigned char eta;		 /* ETA flag */
	unsigned char fineta;		 /* absolute ETA flag */
	unsigned char rate;		 /* rate counter flag */
	unsigned char average_rate;	 /* average rate counter flag */
	unsigned char bytes;		 /* bytes transferred flag */
	unsigned char bufpercent;	 /* transfer buffer percentage flag */
	unsigned int lastwritten;	 /* last-written bytes count */
	unsigned long long rate_limit;	 /* rate limit, in bytes per second */
	unsigned long long buffer_size;	 /* buffer size, in bytes (0=default) */
	unsigned long long size;	 /* total size of data */
	double interval;		 /* interval between updates */
	unsigned int width;		 /* screen width */
	unsigned int height;		 /* screen height */
	char name[256];
	char format[256];
};


static int remote__msgid = -1;


/*
 * Return a key for use with msgget() which will be unique to the current
 * user.
 *
 * We can't just use ftok() because the queue needs to be user-specific
 * so that a user cannot send messages to another user's process, and we
 * can't easily find out the terminal a given process is connected to in a
 * cross-platform way.
 */
static key_t remote__genkey(void)
{
	int uid;
	key_t key;

	uid = geteuid();
	if (uid < 0)
		uid = 0;

	key = ftok("/tmp", 'P') | uid;

	return key;
}


/*
 * Return a message queue ID that is unique to the current user and the
 * given process ID, or -1 on error.
 */
static int remote__msgget(void)
{
	/* Catch SIGSYS in case msgget() raises it, so we get ENOSYS */
	signal(SIGSYS, SIG_IGN);
	return msgget(remote__genkey(), IPC_CREAT | 0600);
}


/*
 * Set the options of a remote process by setting up an IPC message queue,
 * sending a message containing the new options, and then waiting for the
 * message to be consumed by the remote process.
 *
 * Returns nonzero on error.
 */
int pv_remote_set(opts_t opts)
{
	struct remote_msg msgbuf;
	struct msqid_ds qbuf;
	long timeout;
	int msgid;
	long initial_qnum;

	/*
	 * Check that the remote process exists.
	 */
	if (kill(opts->remote, 0) != 0) {
		fprintf(stderr, "%s: %d: %s\n", opts->program_name,
			opts->remote, strerror(errno));
		return 1;
	}

	/*
	 * Make sure parameters are within sensible bounds.
	 */
	if (opts->width < 0)
		opts->width = 80;
	if (opts->height < 0)
		opts->height = 25;
	if (opts->width > 999999)
		opts->width = 999999;
	if (opts->height > 999999)
		opts->height = 999999;
	if ((opts->interval != 0) && (opts->interval < 0.1))
		opts->interval = 0.1;
	if (opts->interval > 600)
		opts->interval = 600;

	/*
	 * Copy parameters into message buffer.
	 */
	memset(&msgbuf, 0, sizeof(msgbuf));
	msgbuf.mtype = opts->remote;
	msgbuf.progress = opts->progress;
	msgbuf.timer = opts->timer;
	msgbuf.eta = opts->eta;
	msgbuf.fineta = opts->fineta;
	msgbuf.rate = opts->rate;
	msgbuf.average_rate = opts->average_rate;
	msgbuf.bytes = opts->bytes;
	msgbuf.bufpercent = opts->bufpercent;
	msgbuf.lastwritten = opts->lastwritten;
	msgbuf.rate_limit = opts->rate_limit;
	msgbuf.buffer_size = opts->buffer_size;
	msgbuf.size = opts->size;
	msgbuf.interval = opts->interval;
	msgbuf.width = opts->width;
	msgbuf.height = opts->height;
	if (opts->name != NULL) {
		strncpy(msgbuf.name, opts->name, sizeof(msgbuf.name) - 1);
	}
	if (opts->format != NULL) {
		strncpy(msgbuf.format, opts->format,
			sizeof(msgbuf.format) - 1);
	}

	msgid = remote__msgget();
	if (msgid < 0) {
		fprintf(stderr, "%s: %s\n", opts->program_name,
			strerror(errno));
		return 1;
	}

	if (msgctl(msgid, IPC_STAT, &qbuf) < 0) {
		fprintf(stderr, "%s: %s\n", opts->program_name,
			strerror(errno));
		return 1;
	}

	initial_qnum = qbuf.msg_qnum;

	if (msgsnd(msgid, &msgbuf, sizeof(msgbuf) - sizeof(long), 0) != 0) {
		fprintf(stderr, "%s: %s\n", opts->program_name,
			strerror(errno));
		return 1;
	}

	timeout = 1100000;

	while (timeout > 10000) {
		struct timeval tv;

		tv.tv_sec = 0;
		tv.tv_usec = 10000;
		select(0, NULL, NULL, NULL, &tv);
		timeout -= 10000;

		/*
		 * If we can't stat the queue, it must have been deleted.
		 */
		if (msgctl(msgid, IPC_STAT, &qbuf) < 0)
			break;

		/*
		 * If the message count is at or below the message count
		 * before we sent our message, assume it was received.
		 */
		if (qbuf.msg_qnum <= initial_qnum) {
			return 0;
		}
	}

	/*
	 * Message not received - delete it.
	 */
	if (msgctl(msgid, IPC_STAT, &qbuf) >= 0) {
		msgrcv(msgid, &msgbuf, sizeof(msgbuf) - sizeof(long),
		       opts->remote, IPC_NOWAIT);
		/*
		 * If this leaves nothing on the queue, remove the
		 * queue, in case we created one for no reason.
		 */
		if (msgctl(msgid, IPC_STAT, &qbuf) >= 0) {
			if (qbuf.msg_qnum < 1)
				msgctl(msgid, IPC_RMID, &qbuf);
		}
	}

	fprintf(stderr, "%s: %d: %s\n", opts->program_name, opts->remote,
		_("message not received"));
	return 1;
}


/*
 * Check for an IPC remote handling message and, if there is one, replace
 * the current process's options with those being passed in.
 *
 * NB relies on pv_state_set_format() causing the output format to be
 * reparsed.
 */
void pv_remote_check(pvstate_t state)
{
	struct remote_msg msgbuf;
	int got;

	if (remote__msgid < 0)
		return;

	memset(&msgbuf, 0, sizeof(msgbuf));

	got =
	    msgrcv(remote__msgid, &msgbuf, sizeof(msgbuf) - sizeof(long),
		   getpid(), IPC_NOWAIT);
	if (got < 0) {
		/*
		 * If our queue had been deleted, re-create it.
		 */
		if (errno != EAGAIN && errno != ENOMSG) {
			remote__msgid = remote__msgget();
		}
	}
	if (got < 1)
		return;

	debug("%s", "received remote message");

	pv_state_format_string_set(state, NULL);
	pv_state_name_set(state, NULL);

	pv_state_set_format(state, msgbuf.progress, msgbuf.timer,
			    msgbuf.eta, msgbuf.fineta, msgbuf.rate,
			    msgbuf.average_rate,
			    msgbuf.bytes, msgbuf.bufpercent,
			    msgbuf.lastwritten,
			    0 ==
			    msgbuf.name[0] ? NULL : strdup(msgbuf.name));

	if (msgbuf.rate_limit > 0)
		pv_state_rate_limit_set(state, msgbuf.rate_limit);
	if (msgbuf.buffer_size > 0) {
		pv_state_target_buffer_size_set(state, msgbuf.buffer_size);
	}
	if (msgbuf.size > 0)
		pv_state_size_set(state, msgbuf.size);
	if (msgbuf.interval > 0)
		pv_state_interval_set(state, msgbuf.interval);
	if (msgbuf.width > 0)
		pv_state_width_set(state, msgbuf.width);
	if (msgbuf.height > 0)
		pv_state_height_set(state, msgbuf.height);
	if (msgbuf.format[0] != 0)
		pv_state_format_string_set(state, strdup(msgbuf.format));
}


/*
 * Initialise remote message reception handling.
 */
void pv_remote_init(void)
{
	remote__msgid = remote__msgget();
}


/*
 * Clean up after remote message reception handling.
 */
void pv_remote_fini(void)
{
	if (remote__msgid >= 0) {
		struct msqid_ds qbuf;
		msgctl(remote__msgid, IPC_RMID, &qbuf);
	}
}

#else				/* !HAVE_IPC */

/*
 * Dummy stubs for remote control when we don't have IPC.
 */
void pv_remote_init(void)
{
}

void pv_remote_check(pvstate_t state)
{
}

void pv_remote_fini(void)
{
}

int pv_remote_set(pvstate_t state)
{
	fprintf(stderr, "%s\n", _("IPC not supported on this system"));
	return 1;
}

#endif				/* HAVE_IPC */

/* EOF */
