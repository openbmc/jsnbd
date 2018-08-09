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

#include <dirent.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <limits.h>
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
#include <sys/types.h>
#include <sys/un.h>
#include <sys/wait.h>

#include <json.h>

#include "config.h"

struct config {
	char			*name;
	bool			is_default;
	char			*nbd_device;
	struct json_object	*metadata;
};

struct ctx {
	int		sock;
	int		sock_client;
	int		signal_pipe[2];
	char		*sock_path;
	pid_t		nbd_client_pid;
	int		nbd_timeout;
	uint8_t		*buf;
	size_t		bufsize;
	struct config	*configs;
	int		n_configs;
	struct config	*default_config;
	struct config	*config;
};

static const char *conf_path = SYSCONFDIR "/nbd-proxy/config.json";
static const char *state_hook_path = SYSCONFDIR "/nbd-proxy/state.d";
static const char *sockpath_tmpl = RUNSTATEDIR "/nbd.%d.sock";

static const size_t bufsize = 0x20000;
static const int nbd_timeout_default = 30;

#define BUILD_ASSERT_OR_ZERO(c) (sizeof(struct {int:-!(c);}))

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
		char timeout_str[10];
		int fd;

		snprintf(timeout_str, sizeof(timeout_str),
				"%d", ctx->nbd_timeout);

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
				"-t", timeout_str,
				ctx->config->nbd_device,
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

#define join_paths(p1, p2, r) \
	(BUILD_ASSERT_OR_ZERO(sizeof(r) > PATH_MAX) + __join_paths(p1, p2, r))
static int __join_paths(const char *p1, const char *p2, char res[])
{
	size_t len;
	char *pos;

	len = strlen(p1) + 1 + strlen(p2);
	if (len > PATH_MAX)
		return -1;

	pos = res;
	strcpy(pos, p1);
	pos += strlen(p1);
	*pos = '/';
	pos++;
	strcpy(pos, p2);

	return 0;
}

static int run_state_hook(struct ctx *ctx,
		const char *path, const char *name, const char *action)
{
	int status, rc, fd;
	pid_t pid;

	pid = fork();
	if (pid < 0) {
		warn("can't fork to execute hook %s", name);
		return -1;
	}

	if (!pid) {
		close(ctx->sock);
		close(ctx->sock_client);
		close(ctx->signal_pipe[0]);
		close(ctx->signal_pipe[1]);
		fd = open("/dev/null", O_RDWR);
		if (fd < 0)
			exit(EXIT_FAILURE);

		dup2(fd, STDIN_FILENO);
		dup2(fd, STDOUT_FILENO);
		dup2(fd, STDERR_FILENO);
		execl(path, name, action, ctx->config->name, NULL);
		exit(EXIT_FAILURE);
	}

	rc = waitpid(pid, &status, 0);
	if (rc < 0) {
		warn("wait");
		return -1;
	}

	if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
		warnx("hook %s failed", name);
		return -1;
	}

	return 0;
}

static int run_state_hooks(struct ctx *ctx, const char *action)
{
	struct dirent *dirent;
	int rc;
	DIR *dir;

	dir = opendir(state_hook_path);
	if (!dir)
		return 0;

	rc = 0;

	for (dirent = readdir(dir); dirent; dirent = readdir(dir)) {
		char full_path[PATH_MAX+1];
		struct stat statbuf;

		if (dirent->d_name[0] == '.')
			continue;

		rc = fstatat(dirfd(dir), dirent->d_name, &statbuf, 0);
		if (rc)
			continue;

		if (!(S_ISREG(statbuf.st_mode) || S_ISLNK(statbuf.st_mode)))
			continue;

		if (faccessat(dirfd(dir), dirent->d_name, X_OK, 0))
			continue;

		rc = join_paths(state_hook_path, dirent->d_name, full_path);
		if (rc)
			continue;

		rc = run_state_hook(ctx, full_path, dirent->d_name, action);
		if (rc)
			break;
	}

	closedir(dir);

	return rc;
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

static void print_metadata(struct ctx *ctx)
{
	struct json_object *md;
	int i;

	md = json_object_new_object();

	for (i = 0; i < ctx->n_configs; i++) {
		struct config *config = &ctx->configs[i];
		json_object_object_add(md, config->name,
				config->metadata);
	}

	puts(json_object_get_string(md));

	json_object_put(md);
}

static void config_free_one(struct config *config)
{
	if (config->metadata)
		json_object_put(config->metadata);
	free(config->nbd_device);
	free(config->name);
}

static int config_parse_one(struct config *config, const char *name,
		json_object *obj)
{
	struct json_object *tmp, *meta;
	json_bool jrc;

	jrc = json_object_object_get_ex(obj, "nbd-device", &tmp);
	if (!jrc) {
		warnx("config %s doesn't specify a nbd-device", name);
		return -1;
	}

	if (!json_object_is_type(tmp, json_type_string)) {
		warnx("config %s has invalid nbd-device", name);
		return -1;
	}

	config->nbd_device = strdup(json_object_get_string(tmp));
	config->name = strdup(name);

	jrc = json_object_object_get_ex(obj, "default", &tmp);
	config->is_default = jrc && json_object_get_boolean(tmp);

	jrc = json_object_object_get_ex(obj, "metadata", &meta);
	if (jrc && json_object_is_type(meta, json_type_object))
		config->metadata = json_object_get(meta);
	else
		config->metadata = NULL;

	return 0;
}

static void config_free(struct ctx *ctx)
{
	int i;

	for (i = 0; i < ctx->n_configs; i++)
		config_free_one(&ctx->configs[i]);

	free(ctx->configs);
	ctx->n_configs = 0;
}

static int config_init(struct ctx *ctx)
{
	struct json_object *obj, *tmp;
	json_bool jrc;
	int i, rc;

	/* apply defaults */
	ctx->nbd_timeout = nbd_timeout_default;

	obj = json_object_from_file(conf_path);
	if (!obj) {
		warnx("can't read configuration from %s\n", conf_path);
		return -1;
	}

	/* global configuration */
	jrc = json_object_object_get_ex(obj, "timeout", &tmp);
	if (jrc) {
		errno = 0;
		ctx->nbd_timeout = json_object_get_int(tmp);
		if (ctx->nbd_timeout == 0 && errno) {
			warnx("can't parse timeout value");
			goto err_free;
		}
	}

	/* per-config configuration */
	jrc = json_object_object_get_ex(obj, "configurations", &tmp);
	if (!jrc) {
		warnx("no configurations specified");
		goto err_free;
	}

	if (!json_object_is_type(tmp, json_type_object)) {
		warnx("invalid configurations format");
		goto err_free;
	}

	ctx->n_configs = json_object_object_length(tmp);
	ctx->configs = calloc(ctx->n_configs, sizeof(*ctx->configs));

	i = 0;
	json_object_object_foreach(tmp, name, config_json) {
		struct config *config = &ctx->configs[i];

		rc = config_parse_one(config, name, config_json);
		if (rc)
			goto err_free;

		if (config->is_default) {
			if (ctx->default_config) {
				warn("multiple configs flagged as default");
				goto err_free;
			}
			ctx->default_config = config;
		}
		i++;
	}

	json_object_put(obj);

	if (ctx->n_configs == 1)
		ctx->default_config = &ctx->configs[0];

	return 0;

err_free:
	warnx("failed to load config from %s", conf_path);
	json_object_put(obj);
	return -1;
}

static int config_select(struct ctx *ctx, const char *name)
{
	struct config *config;
	int i;

	config = NULL;

	if (!name) {
		/* no config specified: use the default */
		if (!ctx->default_config) {
			warnx("no config specified, and no default");
			return -1;
		}
		config = ctx->default_config;

	} else {
		/* find a matching config... */
		for (i = 0; i < ctx->n_configs; i++) {
			if (!strcmp(ctx->configs[i].name, name)) {
				config = &ctx->configs[i];
				break;
			}
		}

		if (!config) {
			warnx("no such configuration '%s'", name);
			return -1;
		}
	}

	/* ... and apply it */
	ctx->config = config;
	return 0;
}

static const struct option options[] = {
	{ .name = "help",	.val = 'h' },
	{ .name = "metadata",	.val = 'm' },
	{ 0 },
};

enum action {
	ACTION_PROXY,
	ACTION_METADATA,
};

static void print_usage(const char *progname)
{
	fprintf(stderr, "usage:\n");
	fprintf(stderr, "\t%s [configuration]\n", progname);
	fprintf(stderr, "\t%s --metadata\n", progname);
}

int main(int argc, char **argv)
{
	enum action action = ACTION_PROXY;
	const char *config_name;
	struct ctx _ctx, *ctx;
	int rc;

	config_name = NULL;

	for (;;) {
		int c = getopt_long(argc, argv, "h", options, NULL);
		if (c == -1)
			break;

		switch (c) {
		case 'm':
			action = ACTION_METADATA;
			break;
		case 'h':
		case '?':
			print_usage(argv[0]);
			return c == 'h' ? EXIT_SUCCESS : EXIT_FAILURE;
		}
	}

	if (optind < argc)
		config_name = argv[optind];

	ctx = &_ctx;
	memset(ctx, 0, sizeof(*ctx));
	ctx->bufsize = bufsize;
	ctx->buf = malloc(ctx->bufsize);

	rc = config_init(ctx);
	if (rc)
		goto out_free;

	if (action == ACTION_METADATA) {
		print_metadata(ctx);
		goto out_free;
	}

	rc = config_select(ctx, config_name);
	if (rc)
		goto out_free;

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

	rc = run_state_hooks(ctx, "start");
	if (rc)
		goto out_stop_client;

	rc = run_proxy(ctx);

	run_state_hooks(ctx, "stop");

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
	config_free(ctx);
	free(ctx->buf);
	return rc ? EXIT_FAILURE : EXIT_SUCCESS;
}
