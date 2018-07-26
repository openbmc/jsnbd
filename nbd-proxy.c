/* Copyright 2018 IBM Corp.
 *
 * Author: Jeremy Kerr <jk@ozlabs.org>
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may not
 * use this file except in compliance with the License.  You may obtain a copy
 * of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 * License for the specific language governing permissions and limitations
 * under the License.
 */

#define _GNU_SOURCE

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/poll.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <sys/wait.h>

#include "config.h"

struct ctx {
	int		sock;
	int		sock_client;
	int		signal_pipe[2];
	char		*sock_path;
	pid_t		nbd_client_pid;
	uint8_t		*buf;
	size_t		bufsize;
};

static const char *sockpath_tmpl = RUNSTATEDIR "/nbd.%d.sock";
static const char *dev_path = "/dev/nbd0";
static const size_t bufsize = 0x20000;

static int open_nbd_socket(struct ctx *ctx)
{
	struct sockaddr_un addr;
	char *path;
	int sd, rc;

	rc = asprintf(&path, sockpath_tmpl, getpid());
	if (rc < 0)
		return -1;

	sd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (sd < 0) {
		warn("can't create socket");
		goto err_free;
	}

	rc = fchmod(sd, S_IRUSR | S_IWUSR);
	if (rc) {
		warn("can't set permissions on socket");
		goto err_close;
	}

	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, path, sizeof(addr.sun_path));

	rc = bind(sd, (struct sockaddr *)&addr, sizeof(addr));
	if (rc) {
		warn("can't bind to path %s", path);
		goto err_close;
	}

	rc = listen(sd, 1);
	if (rc) {
		warn("can't listen on socket %s", path);
		goto err_unlink;
	}

	ctx->sock = sd;
	ctx->sock_path = path;
	return 0;

err_unlink:
	unlink(path);
err_close:
	close(sd);
err_free:
	free(path);
	return -1;
}

static int start_nbd_client(struct ctx *ctx)
{
	pid_t pid;

	pid = fork();
	if (pid < 0) {
		warn("can't create client process");
		return -1;
	}

	/* child process: run nbd-client in non-fork mode */
	if (pid == 0) {
		int fd;

		fd = open("/dev/null", O_RDWR);
		if (fd < 0)
			err(EXIT_FAILURE, "can't open /dev/null");

		dup2(fd, STDIN_FILENO);
		dup2(fd, STDOUT_FILENO);
		dup2(fd, STDERR_FILENO);
		close(fd);
		close(ctx->sock);

		execlp("nbd-client", "nbd-client",
				"-u", ctx->sock_path,
				"-n",
				dev_path,
				NULL);
		err(EXIT_FAILURE, "can't start ndb client");
	}

	ctx->nbd_client_pid = pid;
	return 0;
}

static void stop_nbd_client(struct ctx *ctx)
{
	int rc;

	if (!ctx->nbd_client_pid)
		return;

	rc = kill(ctx->nbd_client_pid, SIGTERM);
	if (rc)
		return;

	waitpid(ctx->nbd_client_pid, NULL, 0);
	ctx->nbd_client_pid = 0;
}

static int copy_fd(struct ctx *ctx, int fd_in, int fd_out)
{
#ifdef HAVE_SPLICE
	int rc;

	rc = splice(fd_in, NULL, fd_out, NULL, ctx->bufsize, 0);
	if (rc < 0)
		warn("splice");

	return rc;
#else
	size_t len, pos;
	ssize_t rc;

	for (;;) {
		errno = 0;
		rc = read(fd_in, ctx->buf, ctx->bufsize);
		if (rc < 0) {
			if (errno == EINTR)
				continue;
			warn("read failure");
			return -1;
		}
		if (rc == 0)
			return 0;
		break;
	}

	len = rc;

	for (pos = 0; pos < len;) {
		errno = 0;
		rc = write(fd_out, ctx->buf + pos, len - pos);
		if (rc < 0) {
			if (errno == EINTR)
				continue;
			warn("write failure");
			return -1;
		}
		if (rc == 0)
			break;
		pos += rc;
	}

	return pos;
#endif
}

static int signal_pipe_fd = -1;

static void signal_handler(int signal)
{
	int rc;

	rc = write(signal_pipe_fd, &signal, sizeof(signal));

	/* not a lot we can do here but exit... */
	if (rc != sizeof(signal))
		exit(EXIT_FAILURE);
}

static int setup_signals(struct ctx *ctx)
{
	struct sigaction sa;
	int rc;

	rc = pipe(ctx->signal_pipe);
	if (rc) {
		warn("cant setup signal pipe");
		return -1;
	}

	signal_pipe_fd = ctx->signal_pipe[1];

	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = signal_handler;

	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGTERM, &sa, NULL);
	sigaction(SIGCHLD, &sa, NULL);

	return 0;
}

static void cleanup_signals(struct ctx *ctx)
{
	struct sigaction sa;
	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = SIG_DFL;

	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGTERM, &sa, NULL);
	sigaction(SIGCHLD, &sa, NULL);

	close(ctx->signal_pipe[0]);
	close(ctx->signal_pipe[1]);
}

static int process_signal_pipe(struct ctx *ctx, bool *exit)
{
	int buf, rc, status;

	rc = read(ctx->signal_pipe[0], &buf, sizeof(buf));
	if (rc != sizeof(buf))
		return -1;

	*exit = false;

	switch (buf) {
	case SIGCHLD:
		rc = waitpid(ctx->nbd_client_pid, &status, WNOHANG);
		if (rc > 0) {
			warnx("nbd client stopped (%s: %d); exiting",
					WIFEXITED(status) ? "rc" : "sig",
					WIFEXITED(status) ?
						WEXITSTATUS(status) :
						WTERMSIG(status));
			ctx->nbd_client_pid = 0;
		}
		break;
	case SIGINT:
	case SIGTERM:
		*exit = true;
		break;
	}

	return 0;
}

static int wait_for_nbd_client(struct ctx *ctx)
{
	struct pollfd pollfds[2];
	int rc;

	pollfds[0].fd = ctx->sock;
	pollfds[0].events = POLLIN;
	pollfds[1].fd = ctx->signal_pipe[0];
	pollfds[1].events = POLLIN;

	for (;;) {
		errno = 0;
		rc = poll(pollfds, 2, -1);
		if (rc < 0) {
			if (errno == EINTR)
				continue;
			warn("poll failed");
			return -1;
		}

		if (pollfds[0].revents) {
			rc = accept(ctx->sock, NULL, NULL);
			if (rc < 0) {
				warn("can't create connection");
				return -1;
			}
			ctx->sock_client = rc;
			break;
		}

		if (pollfds[1].revents) {
			bool exit;
			rc = process_signal_pipe(ctx, &exit);
			if (rc || exit)
				return -1;
		}
	}

	return 0;
}


static int run_proxy(struct ctx *ctx)
{
	struct pollfd pollfds[3];
	bool exit = false;
	int rc;

	/* main proxy: forward data between stdio & socket */
	pollfds[0].fd = ctx->sock_client;
	pollfds[0].events = POLLIN;
	pollfds[1].fd = STDIN_FILENO;
	pollfds[1].events = POLLIN;
	pollfds[2].fd = ctx->signal_pipe[0];
	pollfds[2].events = POLLIN;

	for (;;) {
		errno = 0;
		rc = poll(pollfds, 3, -1);
		if (rc < 0) {
			if (errno == EINTR)
				continue;
			warn("poll failed");
			break;
		}

		if (pollfds[0].revents) {
			rc = copy_fd(ctx, ctx->sock_client, STDOUT_FILENO);
			if (rc <= 0)
				break;
		}

		if (pollfds[1].revents) {
			rc = copy_fd(ctx, STDIN_FILENO, ctx->sock_client);
			if (rc <= 0)
				break;
		}

		if (pollfds[2].revents) {
			rc = process_signal_pipe(ctx, &exit);
			if (rc || exit)
				break;
		}
	}

	return rc ? -1 : 0;
}

int main(void)
{
	struct ctx _ctx, *ctx;
	int rc;

	ctx = &_ctx;
	ctx->bufsize = bufsize;
	ctx->buf = malloc(ctx->bufsize);
	ctx->sock_path = NULL;
	ctx->nbd_client_pid = 0;

	rc = open_nbd_socket(ctx);
	if (rc)
		goto out_free;

	rc = setup_signals(ctx);
	if (rc)
		goto out_close;

	rc = start_nbd_client(ctx);
	if (rc)
		goto out_stop_client;

	rc = wait_for_nbd_client(ctx);
	if (rc)
		goto out_stop_client;

	rc = run_proxy(ctx);

out_stop_client:
	/* we cleanup signals before stopping the client, because we
	 * no longer care about SIGCHLD from the stopping nbd-client
	 * process. stop_nbd_client will be a no-op if the client hasn't
	 * been started. */
	cleanup_signals(ctx);

	stop_nbd_client(ctx);
	close(ctx->sock_client);

out_close:
	if (ctx->sock_path) {
		unlink(ctx->sock_path);
		free(ctx->sock_path);
	}
	close(ctx->sock);
out_free:
	free(ctx->buf);
	return rc ? EXIT_FAILURE : EXIT_SUCCESS;
}
