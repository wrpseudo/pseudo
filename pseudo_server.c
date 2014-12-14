/*
 * pseudo_server.c, pseudo's server-side logic and message handling

 *
 * Copyright (c) 2008-2010, 2013 Wind River Systems, Inc.
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

typedef struct {
	int fd;
	pid_t pid;
	char *tag;
	char *program;
} pseudo_client_t;

pseudo_client_t *clients;

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

static int messages = 0, responses = 0;
static struct timeval message_time = { .tv_sec = 0 };

static void pseudo_server_loop(void);

static int
pseudo_server_write_pid(pid_t pid) {
	char *pseudo_path;
	FILE *fp;

	pseudo_path = pseudo_localstatedir_path(PSEUDO_PIDFILE);
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
	fprintf(fp, "%lld\n", (long long) pid);
	fclose(fp);
	free(pseudo_path);
	return 0;
}

int
pseudo_server_start(int daemonize) {
	struct sockaddr_un sun = { .sun_family = AF_UNIX, .sun_path = PSEUDO_SOCKET };
	char *pseudo_path;
	int rc, newfd;

#if PSEUDO_PORT_DARWIN
	sun.sun_len = strlen(PSEUDO_SOCKET) + 1;
#endif

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
	pseudo_path = pseudo_localstatedir_path(NULL);
	if (!pseudo_path || chdir(pseudo_path) == -1) {
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
	if (daemonize) {
		if ((rc = fork()) != 0) {
			if (rc == -1) {
				pseudo_diag("couldn't spawn server: %s\n", strerror(errno));
				return 0;
			}
			pseudo_debug(PDBGF_SERVER, "started server, pid %d\n", rc);
			close(listen_fd);
			/* Parent writes pid, that way it's always correct */
			return pseudo_server_write_pid(rc);
		}
		/* In child */
		pseudo_new_pid();
		fclose(stdin);
		fclose(stdout);
		pseudo_logfile(PSEUDO_LOGFILE);
	} else {
		/* Write the pid if we don't daemonize */
		pseudo_server_write_pid(getpid());
	}

	setsid();
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
	pseudo_client_t *new_clients;
	int i;

	/* if possible, use first open client slot */
	for (i = 0; i < max_clients; ++i) {
		if (clients[i].fd == -1) {
			pseudo_debug(PDBGF_SERVER, "reusing client %d for fd %d\n", i, fd);
			clients[i].fd = fd;
			clients[i].pid = 0;
			clients[i].tag = NULL;
			clients[i].program = NULL;
			++active_clients;
			if (i > highest_client)
				highest_client = i;
			return;
		}
	}

	/* otherwise, allocate a new one */
	new_clients = malloc(sizeof(*new_clients) * (max_clients + 16));
	if (new_clients) {
		memcpy(new_clients, clients, max_clients * sizeof(*clients));
		free(clients);
		for (i = max_clients; i < max_clients + 16; ++i) {
			new_clients[i].fd = -1;
			new_clients[i].pid = 0;
			new_clients[i].tag = NULL;
			new_clients[i].program = NULL;
		}
		clients = new_clients;
		clients[max_clients].fd = fd;
		clients[max_clients].pid = 0;
		clients[max_clients].tag = NULL;
		clients[max_clients].program = NULL;
		highest_client = max_clients + 1;

		max_clients += 16;
		++active_clients;
	} else {
		pseudo_diag("error allocating new client, fd %d\n", fd);
		close(fd);
	}
}

/* clear pid/fd.  If this was the highest client, iterate downwards looking
 * for a lower one to be the new highest client.
 */
static void
close_client(int client) {
	pseudo_debug(PDBGF_SERVER, "lost client %d [%d], closing fd %d\n", client,
		clients[client].pid, clients[client].fd);
	/* client went away... */
	if (client > highest_client || client <= 0) {
		pseudo_diag("tried to close client %d (highest is %d)\n",
			client, highest_client);
		return;
	}
	close(clients[client].fd);
	clients[client].fd = -1;
	free(clients[client].tag);
	free(clients[client].program);
	clients[client].pid = 0;
	clients[client].tag = NULL;
	clients[client].program = NULL;
	--active_clients;
	if (client == highest_client)
		while (clients[highest_client].fd != -1 && highest_client > 0)
			--highest_client;
}

/* Actually process a request.
 */
static int
serve_client(int i) {
	pseudo_msg_t *in;
	int rc;

	pseudo_debug(PDBGF_SERVER, "message from client %d [%d:%s - %s] fd %d\n",
		i, (int) clients[i].pid,
		clients[i].program ? clients[i].program : "???",
		clients[i].tag ? clients[i].tag : "NO TAG",
		clients[i].fd);
	in = pseudo_msg_receive(clients[i].fd);
	if (in) {
		char *response_path = 0;
		size_t response_pathlen;
                int send_response = 1;
		pseudo_debug(PDBGF_SERVER | PDBGF_VERBOSE, "got a message (%d): %s\n", in->type, (in->pathlen ? in->path : "<no path>"));
		/* handle incoming ping */
		if (in->type == PSEUDO_MSG_PING && !clients[i].pid) {
			pseudo_debug(PDBGF_SERVER, "new client: %d -> %d",
				i, in->client);
			clients[i].pid = in->client;
			if (in->pathlen) {
				size_t proglen;
				proglen = strlen(in->path);

				pseudo_debug(PDBGF_SERVER, " <%s>", in->path);
				free(clients[i].program);
				clients[i].program = malloc(proglen + 1);
				if (clients[i].program) {
					snprintf(clients[i].program, proglen + 1, "%s", in->path);
				}
				if (in->pathlen > proglen) {
					pseudo_debug(PDBGF_SERVER, " [%s]", in->path + proglen + 1);
					clients[i].tag = malloc(in->pathlen - proglen);
					if (clients[i].tag)
						snprintf(clients[i].tag, in->pathlen - proglen,
							"%s", in->path + proglen + 1);
				}
			}
			pseudo_debug(PDBGF_SERVER, "\n");
		}
		/* sanity-check client ID */
		if (in->client != clients[i].pid) {
			pseudo_debug(PDBGF_SERVER, "uh-oh, expected pid %d for client %d, got %d\n",
				(int) clients[i].pid, i, in->client);
		}
		/* regular requests are processed in place by
		 * pseudo_server_response.
		 */
		if (in->type != PSEUDO_MSG_SHUTDOWN) {
                        if (in->type == PSEUDO_MSG_FASTOP)
                                send_response = 0;
			/* most messages don't need these, but xattr may */
			response_path = 0;
			response_pathlen = -1;
			if (pseudo_server_response(in, clients[i].program, clients[i].tag, &response_path, &response_pathlen)) {
				in->type = PSEUDO_MSG_NAK;
			} else {
				in->type = PSEUDO_MSG_ACK;
				pseudo_debug(PDBGF_SERVER | PDBGF_VERBOSE, "response: %d (%s)\n",
					in->result, pseudo_res_name(in->result));
			}
			in->client = i;
			if (response_path) {
				in->pathlen = response_pathlen;
			} else {
				in->pathlen = 0;
			}
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
					if (clients[j].fd != -1 && j != i) {
						s += snprintf(s, 8, "%d ", (int) clients[j].pid);
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
                if (send_response) {
                        if ((rc = pseudo_msg_send(clients[i].fd, in, in->pathlen, response_path)) != 0) {
                                pseudo_debug(PDBGF_SERVER, "failed to send response to client %d [%d]: %d (%s)\n",
                                        i, (int) clients[i].pid, rc, strerror(errno));
                        }
                } else {
                        rc = 1;
                }
		free(response_path);
		return rc;
	} else {
		/* this should not be happening, but the exceptions aren't
		 * being detected in select() for some reason.
		 */
		pseudo_debug(PDBGF_SERVER, "client %d: no message\n", (int) clients[i].pid);
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

	clients = malloc(16 * sizeof(*clients));

	clients[0].fd = listen_fd;
	clients[0].pid = getpid();

	for (i = 1; i < 16; ++i) {
		clients[i].fd = -1;
		clients[i].pid = 0;
		clients[i].tag = NULL;
		clients[i].program = NULL;
	}

	active_clients = 1;
	max_clients = 16;
	highest_client = 0;

	pseudo_debug(PDBGF_SERVER, "server loop started.\n");
	if (listen_fd < 0) {
		pseudo_diag("got into loop with no valid listen fd.\n");
		exit(1);
	}
	pdb_log_msg(SEVERITY_INFO, NULL, NULL, NULL, "server started (pid %d)", getpid());

	FD_ZERO(&reads);
	FD_ZERO(&writes);
	FD_ZERO(&events);
	FD_SET(clients[0].fd, &reads);
	FD_SET(clients[0].fd, &events);
	max_fd = clients[0].fd;
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
                                /* maybe flush database to disk */
                                pdb_maybe_backup();
				if (loop_timeout <= 0) {
					pseudo_debug(PDBGF_SERVER, "no more clients, got bored.\n");
					die_peacefully = 1;
				} else {
					/* display this if not exiting */
					pseudo_debug(PDBGF_SERVER | PDBGF_BENCHMARK, "%d messages handled in %.4f seconds, %d responses\n",
						messages,
						(double) message_time.tv_sec +
						(double) message_time.tv_usec / 1000000.0,
                                                responses);
				}
			}
		} else if (rc > 0) {
			loop_timeout = pseudo_server_timeout;
			for (i = 1; i <= highest_client; ++i) {
				if (clients[i].fd == -1)
					continue;
				if (FD_ISSET(clients[i].fd, &events)) {
					/* this should happen but doesn't... */
					close_client(i);
				} else if (FD_ISSET(clients[i].fd, &reads)) {
					struct timeval tv1, tv2;
                                        int rc;
					gettimeofday(&tv1, NULL);
					rc = serve_client(i);
					gettimeofday(&tv2, NULL);
					++messages;
                                        if (rc == 0)
                                                ++responses;
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
			    (FD_ISSET(clients[0].fd, &events) ||
			     FD_ISSET(clients[0].fd, &reads))) {
				len = sizeof(client);
				if ((fd = accept(listen_fd, (struct sockaddr *) &client, &len)) != -1) {
					pseudo_debug(PDBGF_SERVER, "new client fd %d\n", fd);
					open_client(fd);
				}
			}
			pseudo_debug(PDBGF_SERVER, "server loop complete [%d clients left]\n", active_clients);
		}
		if (die_peacefully || die_forcefully) {
			pseudo_debug(PDBGF_SERVER, "quitting.\n");
			pseudo_debug(PDBGF_SERVER | PDBGF_BENCHMARK, "server %d exiting: handled %d messages in %.4f seconds\n",
				getpid(), messages,
				(double) message_time.tv_sec +
				(double) message_time.tv_usec / 1000000.0);
			pdb_log_msg(SEVERITY_INFO, NULL, NULL, NULL, "server %d exiting: handled %d messages in %.4f seconds",
				getpid(), messages,
				(double) message_time.tv_sec +
				(double) message_time.tv_usec / 1000000.0);
			close(clients[0].fd);
			exit(0);
		}
		FD_ZERO(&reads);
		FD_ZERO(&writes);
		FD_ZERO(&events);
		FD_SET(clients[0].fd, &reads);
		FD_SET(clients[0].fd, &events);
		max_fd = clients[0].fd;
		/* current_clients is a sanity check; note that for
		 * purposes of select(), the server is one of the fds,
		 * and thus, "a client".
		 */
		current_clients = 1;
		for (i = 1; i <= highest_client; ++i) {
			if (clients[i].fd != -1) {
				++current_clients;
				FD_SET(clients[i].fd, &reads);
				FD_SET(clients[i].fd, &events);
				if (clients[i].fd > max_fd)
					max_fd = clients[i].fd;
			}
		}
		if (current_clients != active_clients) {
			pseudo_debug(PDBGF_SERVER, "miscount of current clients (%d) against active_clients (%d)?\n",
				current_clients, active_clients);
		}
		/* reinitialize timeout because Linux select alters it */
		timeout = (struct timeval) { .tv_sec = LOOP_DELAY, .tv_usec = 0 };
	}
	pseudo_diag("select failed: %s\n", strerror(errno));
}
