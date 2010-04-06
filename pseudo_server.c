/*
 * pseudo_server.c, pseudo's server-side logic and message handling
 *
 * Copyright (c) 2008-2010 Wind River Systems, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the Lesser GNU General Public License version 2.1 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the Lesser GNU General Public License for more details.
 *
 * You should have received a copy of the Lesser GNU General Public License
 * version 2.1 along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA 
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>

#include <unistd.h>
#include <fcntl.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <signal.h>

#include "pseudo.h"
#include "pseudo_ipc.h"
#include "pseudo_server.h"
#include "pseudo_client.h"
#include "pseudo_db.h"

static int listen_fd = -1;

int *client_fds;
pid_t *client_pids;
char **client_tags;
/* active_clients: Number of clients we actually have right now.
 * highest_client: Highest index into clients table of an active client.
 * max_clients: Size of table.
 */
static int active_clients = 0, highest_client = 0, max_clients = 0;

#define LOOP_DELAY 2
int pseudo_server_timeout = 30;
static int die_peacefully = 0;
static int die_forcefully = 0;

/* when the client is linked with pseudo_wrappers, these are defined there.
 * when it is linked with pseudo_server, though, we have to provide different
 * versions (pseudo_wrappers must not be linked with the server, or Bad Things
 * happen).
 */
void pseudo_magic(void) { }
void pseudo_antimagic(void) { }

void
quit_now(int signal) {
	pseudo_diag("Received signal %d, quitting.\n", signal);
	die_forcefully = 1;
}

static int messages = 0;
static struct timeval message_time = { .tv_sec = 0 };

static void pseudo_server_loop(void);

int
pseudo_server_start(int daemonize) {
	struct sockaddr_un sun = { AF_UNIX, "pseudo.socket" };
	char *pseudo_path;
	int rc, newfd;
	FILE *fp;

	listen_fd = socket(PF_UNIX, SOCK_STREAM, 0);
	if (listen_fd < 0) {
		pseudo_diag("couldn't create listening socket: %s\n", strerror(errno));
		return 1;
	}

	if (listen_fd <= 2) {
		newfd = fcntl(listen_fd, F_DUPFD, 3);
		if (newfd < 0) {
			pseudo_diag("couldn't dup listening socket: %s\n", strerror(errno));
			close(listen_fd);
			return 1;
		} else {
			close(listen_fd);
			listen_fd = newfd;
		}
	}

	/* cd to the data directory */
	pseudo_path = pseudo_prefix_path(PSEUDO_DATA);
	if (!pseudo_path) {
		pseudo_diag("can't find prefix/%s directory.\n", PSEUDO_DATA);
		return 1;
	}
	if (chdir(pseudo_path) == -1) {
		pseudo_diag("can't get to '%s': %s\n",
			pseudo_path, strerror(errno));
		return 1;
	}
	free(pseudo_path);
	/* remove existing socket -- if it exists */
	unlink(sun.sun_path);
	if (bind(listen_fd, (struct sockaddr *) &sun, sizeof(sun)) == -1) {
		pseudo_diag("couldn't bind listening socket: %s\n", strerror(errno));
		return 1;
	}
	if (listen(listen_fd, 5) == -1) {
		pseudo_diag("couldn't listen on socket: %s\n", strerror(errno));
		return 1;
	}
	if (daemonize && ((rc = fork()) != 0)) {
		if (rc == -1) {
			pseudo_diag("couldn't spawn server: %s\n", strerror(errno));
			return 0;
		}
		pseudo_debug(2, "started server, pid %d\n", rc);
		close(listen_fd);
		return 0;
	}
	setsid();
	pseudo_path = pseudo_prefix_path(PSEUDO_PIDFILE);
	if (!pseudo_path) {
		pseudo_diag("Couldn't get path for prefix/%s\n", PSEUDO_PIDFILE);
		return 1;
	}
	fp = fopen(pseudo_path, "w");
	if (!fp) {
		pseudo_diag("Couldn't open %s: %s\n",
			pseudo_path, strerror(errno));
		return 1;
	}
	fprintf(fp, "%lld\n", (long long) getpid());
	fclose(fp);
	free(pseudo_path);
	if (daemonize) {
		int fd;

		pseudo_new_pid();
		fclose(stdin);
		fclose(stdout);
		pseudo_path = pseudo_prefix_path(PSEUDO_LOGFILE);
		if (!pseudo_path) {
			pseudo_diag("can't get path for prefix/%s\n", PSEUDO_LOGFILE);
			return 1;
		}
		fd = open(pseudo_path, O_WRONLY | O_APPEND | O_CREAT, 0644);
		if (fd == -1) {
			pseudo_diag("help: can't open %s: %s\n", PSEUDO_LOGFILE, strerror(errno));
		} else {
			pseudo_util_debug_fd = fd;
			fclose(stderr);
		}
		free(pseudo_path);
	}
	signal(SIGHUP, quit_now);
	signal(SIGINT, quit_now);
	signal(SIGALRM, quit_now);
	signal(SIGQUIT, quit_now);
	signal(SIGTERM, quit_now);
	pseudo_server_loop();
	return 0;
}

/* mess with internal tables as needed */
static void
open_client(int fd) {
	int *new_client_fds;
	pid_t *new_client_pids;
	char **new_client_tags;
	int i;

	/* if possible, use first open client slot */
	for (i = 0; i < max_clients; ++i) {
		if (client_fds[i] == -1) {
			pseudo_debug(2, "reusing client %d for fd %d\n", i, fd);
			client_fds[i] = fd;
			client_pids[i] = 0;
			client_tags[i] = 0;
			++active_clients;
			if (i > highest_client)
				highest_client = i;
			return;
		}
	}

	/* otherwise, allocate a new one */
	new_client_fds = malloc(sizeof(int) * (max_clients + 16));
	new_client_pids = malloc(sizeof(pid_t) * (max_clients + 16));
	new_client_tags = malloc(sizeof(char *) * (max_clients + 16));
	if (new_client_fds && new_client_pids && new_client_tags) {
		memcpy(new_client_fds, client_fds, max_clients * sizeof(int));
		memcpy(new_client_pids, client_pids, max_clients * sizeof(pid_t));
		memcpy(new_client_tags, client_tags, max_clients * sizeof(char *));
		free(client_fds);
		free(client_pids);
		free(client_tags);
		for (i = max_clients; i < max_clients + 16; ++i) {
			new_client_fds[i] = -1;
			new_client_pids[i] = 0;
			new_client_tags[i] = 0;
		}
		client_fds = new_client_fds;
		client_pids = new_client_pids;
		client_tags = new_client_tags;

		client_fds[max_clients] = fd;
		client_pids[max_clients] = 0;
		client_tags[max_clients] = 0;
		highest_client = max_clients + 1;

		max_clients += 16;
		++active_clients;
	} else {
		/* if something got allocated, free it */
		free(new_client_fds);
		free(new_client_pids);
		free(new_client_tags);
		pseudo_diag("error allocating new client, fd %d\n", fd);
		close(fd);
	}
}

/* clear pid/fd.  If this was the highest client, iterate downwards looking
 * for a lower one to be the new highest client.
 */
static void
close_client(int client) {
	pseudo_debug(2, "lost client %d [%d], closing fd %d\n", client, client_pids[client], client_fds[client]);
	/* client went away... */
	close(client_fds[client]);
	client_fds[client] = -1;
	free(client_tags[client]);
	client_tags[client] = 0;
	client_pids[client] = 0;
	--active_clients;
	if (client == highest_client)
		while (client_fds[highest_client] != -1 && highest_client > 0)
			--highest_client;
}

/* Actually process a request.
 */
static int
serve_client(int i) {
	pseudo_msg_t *in;
	int rc;

	pseudo_debug(2, "message from client %d [%d:%s] fd %d\n",
		i, (int) client_pids[i],
		client_tags[i] ? client_tags[i] : "NO TAG",
		client_fds[i]);
	in = pseudo_msg_receive(client_fds[i]);
	if (in) {
		char *response_path = 0;
		pseudo_debug(4, "got a message (%d): %s\n", in->type, (in->pathlen ? in->path : "<no path>"));
		/* handle incoming ping */
		if (in->type == PSEUDO_MSG_PING && !client_pids[i]) {
			pseudo_debug(2, "new client: %d -> %d\n",
				i, in->client);
			client_pids[i] = in->client;
			if (in->pathlen) {
				free(client_tags[i]);
				client_tags[i] = strdup(in->path);
			}
		}
		/* sanity-check client ID */
		if (in->client != client_pids[i]) {
			pseudo_debug(1, "uh-oh, expected pid %d for client %d, got %d\n",
				(int) client_pids[i], i, in->client);
		}
		/* regular requests are processed in place by
		 * pseudo_server_response.
		 */
		if (in->type != PSEUDO_MSG_SHUTDOWN) {
			if (pseudo_server_response(in, client_tags[i])) {
				in->type = PSEUDO_MSG_NAK;
			} else {
				in->type = PSEUDO_MSG_ACK;
				pseudo_debug(4, "response: %d (%s)\n",
					in->result, pseudo_res_name(in->result));
			}
			/* no path in response */
			in->pathlen = 0;
			in->client = i;
		} else {
			/* the server's listen fd is "a client", and
			 * so is the program connecting to request a shutdown.
			 * it should never be less than 2, but crazy things
			 * happen.  >2 implies some other active client,
			 * though.
			 */
			if (active_clients > 2) {
				int j;
				char *s;

				response_path = malloc(8 * active_clients);
				in->type = PSEUDO_MSG_NAK;
				in->fd = active_clients - 2;
				s = response_path;
				for (j = 1; j <= highest_client; ++j) {
					if (client_fds[j] != -1 && j != i) {
						s += snprintf(s, 8, "%d ", (int) client_pids[j]);
					}
				}
				in->pathlen = (s - response_path) + 1;
				/* exit quickly once clients go away, though */
				pseudo_server_timeout = 1;
			} else {
				in->type = PSEUDO_MSG_ACK;
				in->pathlen = 0;
				in->client = i;
				die_peacefully = 1;
			}
		}
		if ((rc = pseudo_msg_send(clients[i].fd, in, -1, response_path)) != 0)
			pseudo_debug(1, "failed to send response to client %d [%d]: %d (%s)\n",
				i, (int) client_pids[i], rc, strerror(errno));
		rc = in->op;
		free(response_path);
		return rc;
	} else {
		/* this should not be happening, but the exceptions aren't
		 * being detected in select() for some reason.
		 */
		pseudo_debug(2, "client %d: no message\n", (int) client_pids[i]);
		close_client(i);
		return 0;
	}
}

/* get clients, handle messages, shut down.
 * This doesn't actually do any work, it just calls a ton of things which
 * do work.
 */
static void
pseudo_server_loop(void) {
	struct sockaddr_un client;
	socklen_t len;
	fd_set reads, writes, events;
	int max_fd, current_clients;
	struct timeval timeout;
	int i;
	int rc;
	int fd;
	int loop_timeout = pseudo_server_timeout;

	client_fds = malloc(sizeof(int) * 16);
	client_pids = malloc(sizeof(pid_t) * 16);
	client_tags = malloc(sizeof(char *) * 16);

	client_fds[0] = listen_fd;
	client_pids[0] = getpid();

	for (i = 1; i < 16; ++i) {
		client_fds[i] = -1;
		client_pids[i] = 0;
		client_tags[i] = 0;
	}

	active_clients = 1;
	max_clients = 16;
	highest_client = 0;

	pseudo_debug(2, "server loop started.\n");
	if (listen_fd < 0) {
		pseudo_diag("got into loop with no valid listen fd.\n");
		exit(1);
	}
	pdb_log_msg(SEVERITY_INFO, NULL, NULL, "server started (pid %d)", getpid());

	FD_ZERO(&reads);
	FD_ZERO(&writes);
	FD_ZERO(&events);
	FD_SET(client_fds[0], &reads);
	FD_SET(client_fds[0], &events);
	max_fd = client_fds[0];
	timeout = (struct timeval) { .tv_sec = LOOP_DELAY, .tv_usec = 0 };
	
	/* EINTR tends to come from profiling, so it is not a good reason to
	 * exit; other signals are caught and set the flag causing a graceful
	 * exit. */
	while ((rc = select(max_fd + 1, &reads, &writes, &events, &timeout)) >= 0 || (errno == EINTR)) {
		if (rc == 0 || (rc == -1 && errno == EINTR)) {
			/* If there's no clients, start timing out.  If there
			 * are active clients, never time out.
			 */
			if (active_clients == 1) {
				loop_timeout -= LOOP_DELAY;
				if (loop_timeout <= 0) {
					pseudo_debug(1, "no more clients, got bored.\n");
					die_peacefully = 1;
				} else {
					/* display this if not exiting */
					pseudo_debug(1, "%d messages handled in %.4f seconds\n",
						messages,
						(double) message_time.tv_sec +
						(double) message_time.tv_usec / 1000000.0);
				}
			}
		} else if (rc > 0) {
			loop_timeout = pseudo_server_timeout;
			for (i = 1; i <= highest_client; ++i) {
				if (client_fds[i] == -1)
					continue;
				if (FD_ISSET(client_fds[i], &events)) {
					/* this should happen but doesn't... */
					close_client(i);
				} else if (FD_ISSET(client_fds[i], &reads)) {
					struct timeval tv1, tv2;
					int op;
					gettimeofday(&tv1, NULL);
					op = serve_client(i);
					gettimeofday(&tv2, NULL);
					++messages;
					message_time.tv_sec += (tv2.tv_sec - tv1.tv_sec);
					message_time.tv_usec += (tv2.tv_usec - tv1.tv_usec);
					if (message_time.tv_usec < 0) {
						message_time.tv_usec += 1000000;
						--message_time.tv_sec;
					} else while (message_time.tv_usec > 1000000) {
						message_time.tv_usec -= 1000000;
						++message_time.tv_sec;
					}
				}
				if (die_forcefully)
					break;
			}
			if (!(die_peacefully || die_forcefully) && 
			    (FD_ISSET(client_fds[0], &events) ||
			     FD_ISSET(client_fds[0], &reads))) {
				len = sizeof(client);
				if ((fd = accept(listen_fd, (struct sockaddr *) &client, &len)) != -1) {
					pseudo_debug(2, "new client fd %d\n", fd);
					open_client(fd);
				}
			}
			pseudo_debug(2, "server loop complete [%d clients left]\n", active_clients);
		}
		if (die_peacefully || die_forcefully) {
			pseudo_debug(2, "quitting.\n");
			pseudo_debug(1, "server %d exiting: handled %d messages in %.4f seconds\n",
				getpid(), messages,
				(double) message_time.tv_sec +
				(double) message_time.tv_usec / 1000000.0);
			pdb_log_msg(SEVERITY_INFO, NULL, NULL, "server %d exiting: handled %d messages in %.4f seconds",
				getpid(), messages,
				(double) message_time.tv_sec +
				(double) message_time.tv_usec / 1000000.0);
			close(client_fds[0]);
			exit(0);
		}
		FD_ZERO(&reads);
		FD_ZERO(&writes);
		FD_ZERO(&events);
		FD_SET(client_fds[0], &reads);
		FD_SET(client_fds[0], &events);
		max_fd = client_fds[0];
		/* current_clients is a sanity check; note that for
		 * purposes of select(), the server is one of the fds,
		 * and thus, "a client".
		 */
		current_clients = 1;
		for (i = 1; i <= highest_client; ++i) {
			if (client_fds[i] != -1) {
				++current_clients;
				FD_SET(client_fds[i], &reads);
				FD_SET(client_fds[i], &events);
				if (client_fds[i] > max_fd)
					max_fd = client_fds[i];
			}
		}
		if (current_clients != active_clients) {
			pseudo_debug(1, "miscount of current clients (%d) against active_clients (%d)?\n",
				current_clients, active_clients);
		}
		/* reinitialize timeout because Linux select alters it */
		timeout = (struct timeval) { .tv_sec = LOOP_DELAY, .tv_usec = 0 };
	}
	pseudo_diag("select failed: %s\n", strerror(errno));
}
