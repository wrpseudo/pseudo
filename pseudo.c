/*
 * pseudo.c, main pseudo utility program
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
#include <stdlib.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <time.h>
#include <limits.h>

#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/fcntl.h>
#include <sys/file.h>

#include "pseudo.h"
#include "pseudo_ipc.h"
#include "pseudo_client.h"
#include "pseudo_server.h"
#include "pseudo_db.h"

int opt_C = 0;
int opt_d = 0;
int opt_f = 0;
int opt_l = 0;
long opt_p = 0;
char *opt_r = NULL;
int opt_S = 0;

static int pseudo_op(pseudo_msg_t *msg, const char *program, const char *tag);
static int pseudo_db_check(void);

void
usage(int status) {
	FILE *f = status ? stderr : stdout;
	fputs("Usage: pseudo [-dflv] [-P prefix] [-rR root] [-t timeout] [command]\n", f);
	fputs("       pseudo -h\n", f);
	fputs("       pseudo [-dflv] [-P prefix] -C\n", f);
	fputs("       pseudo [-dflv] [-P prefix] -S\n", f);
	fputs("       pseudo [-dflv] [-P prefix] -V\n", f);
	exit(status);
}

/* main server process */
int
main(int argc, char *argv[]) {
	int o;
	char *s;
	int lockfd, newfd;
	char *ld_env = getenv("LD_PRELOAD");
	int rc;
	char opts[pseudo_path_max()], *optptr = opts;
	char *lockname;

	opts[0] = '\0';

	s = pseudo_get_value("PSEUDO_DEBUG");
	if (s) {
		int level = atoi(s);
		for (o = 0; o < level; ++o) {
			pseudo_debug_verbose();
		}
	}
	free(s);

	if (ld_env && strstr(ld_env, "libpseudo")) {
		pseudo_debug(2, "can't run daemon with libpseudo in LD_PRELOAD\n");
		s = pseudo_get_value("PSEUDO_RELOADED");
		if (s) {
			pseudo_diag("I can't seem to make LD_PRELOAD go away.  Sorry.\n");
			pseudo_diag("LD_PRELOAD: %s\n", ld_env);
			exit(EXIT_FAILURE);
		}
		free(s);
		pseudo_set_value("PSEUDO_RELOADED", "YES");
		pseudo_setupenv();
		pseudo_dropenv(); /* Drop LD_PRELOAD */

		execv(argv[0], argv);
		exit(EXIT_FAILURE);
	}
	pseudo_set_value("PSEUDO_RELOADED", NULL);

	/* we need cwd to canonicalize paths */
	pseudo_client_getcwd();

	/* warning:  GNU getopt permutes arguments, which is just plain
	 * wrong.  The + suppresses this annoying behavior, but may not
	 * be compatible with sane option libraries.
	 */
	while ((o = getopt(argc, argv, "+Cdfhlp:P:r:R:St:vV")) != -1) {
		switch (o) {
		case 'C':
			/* check database */
			opt_C = 1;
			break;
		case 'd':
			/* run as daemon */
			opt_d = 1;
			break;
		case 'f':
			/* run foregrounded */
			opt_f = 1;
			break;
		case 'h':
			usage(0);
			break;
		case 'l':
			optptr += snprintf(optptr, pseudo_path_max() - (optptr - opts),
					"%s-l", optptr > opts ? " " : "");
			opt_l = 1;
			break;
		case 'p':
			s = PSEUDO_ROOT_PATH(AT_FDCWD, optarg, AT_SYMLINK_NOFOLLOW);
			if (!s)
				pseudo_diag("Can't resolve passwd path '%s'\n", optarg);
			pseudo_set_value("PSEUDO_PASSWD", s);
			break;
		case 'P':
			s = PSEUDO_ROOT_PATH(AT_FDCWD, optarg, AT_SYMLINK_NOFOLLOW);
			if (!s)
				pseudo_diag("Can't resolve prefix path '%s'\n", optarg);
			pseudo_set_value("PSEUDO_PREFIX", s);
			break;
		case 'r':	/* FALLTHROUGH */
		case 'R':
			s = PSEUDO_ROOT_PATH(AT_FDCWD, optarg, AT_SYMLINK_NOFOLLOW);
			if (!s)
				pseudo_diag("Can't resolve root path '%s'\n", optarg);
			pseudo_set_value("PSEUDO_CHROOT", s);
			if (o == 'r')
				opt_r = s;
			break;
		case 'S':
			opt_S = 1;
			break;
		case 't':
			pseudo_server_timeout = strtol(optarg, &s, 10);
			if (*s && !isspace(*s)) {
				pseudo_diag("Timeout must be an integer value.\n");
				usage(EXIT_FAILURE);
			}
			optptr += snprintf(optptr, pseudo_path_max() - (optptr - opts),
					"%s-t %d", optptr > opts ? " " : "",
					pseudo_server_timeout);
			break;
		case 'v':
			pseudo_debug_verbose();
			break;
		case 'V':
			printf("pseudo version %s\n", pseudo_version ? pseudo_version : "<undefined>");
			printf("pseudo configuration:\n  prefix: %s\n",
				PSEUDO_PREFIX);
			printf("Set PSEUDO_PREFIX to run with a different prefix.\n");
			exit(0);
			break;
		case '?':
		default:
			pseudo_diag("unknown/invalid argument (option '%c').\n", optopt);
			usage(EXIT_FAILURE);
			break;
		}
	}
	/* Options are processed, preserve them... */
	pseudo_set_value("PSEUDO_OPTS", opts);

	if (!pseudo_get_prefix(argv[0])) {
		pseudo_diag("Can't figure out prefix.  Set PSEUDO_PREFIX or invoke with full path.\n");
		exit(EXIT_FAILURE);
	}

	if (opt_C) {
		return pseudo_db_check();
	}

	if (opt_S) {
		return pseudo_client_shutdown();
	}

	if (opt_d && opt_f) {
		pseudo_diag("You cannot run a foregrounded daemon.\n");
		exit(EXIT_FAILURE);
	}

	if (opt_f || opt_d) {
		if (argc > optind) {
			pseudo_diag("pseudo: running program implies spawning background daemon.\n");
			exit(EXIT_FAILURE);
		}
	} else {
		char fullpath[pseudo_path_max()];
		char *path;

		if (opt_r) {
			if (chdir(opt_r) == -1) {
				pseudo_diag("failed to chdir to '%s': %s\n",
					opt_r, strerror(errno));
				exit(EXIT_FAILURE);
			}
		}
		if (argc > optind) {
			pseudo_debug(2, "running command: %s\n",
				argv[optind]);
			argc -= optind;
			argv += optind;
		} else {
			static char *newargv[2];
			argv = newargv;
			pseudo_debug(2, "running shell.\n");
			argv[0] = getenv("SHELL");
			if (!argv[0])
				argv[0] = "/bin/sh";
			argv[1] = NULL;
		}

		if (strchr(argv[0], '/')) {
			snprintf(fullpath, pseudo_path_max(), "%s", argv[0]);
		} else {
			int found = 0;
			if ((path = getenv("PATH")) == NULL)
				path = "/bin:/usr/bin";
			while (*path) {
				struct stat64 buf;
				int len = strcspn(path, ":");
				snprintf(fullpath, pseudo_path_max(), "%.*s/%s",
					len, path, argv[0]);
				path += len;
				if (*path == ':')
					++path;
				if (!stat64(fullpath, &buf)) {
					if (buf.st_mode & 0111) {
						found = 1;
						break;
					}
				}
			}
			if (!found) {
				pseudo_diag("Can't find '%s' in $PATH.\n",
					argv[0]);
				exit(EXIT_FAILURE);
			}
		}
		pseudo_setupenv();

		rc = execv(fullpath, argv);
		if (rc == -1) {
			pseudo_diag("pseudo: can't run %s: %s\n",
				argv[0], strerror(errno));
		}
		exit(EXIT_FAILURE);
	}
	/* if we got here, we are not running a command, and we are not in
	 * a pseudo environment.
	 */
	pseudo_new_pid();

	pseudo_debug(3, "opening lock.\n");
	lockname = pseudo_localstatedir_path(PSEUDO_LOCKFILE);
	if (!lockname) {
		pseudo_diag("Couldn't allocate a file path.\n");
		exit(EXIT_FAILURE);
	}
	lockfd = open(lockname, O_RDWR | O_CREAT, 0644);
	if (lockfd < 0) {
		pseudo_diag("Can't open or create lockfile %s: %s\n",
			lockname, strerror(errno));
		exit(EXIT_FAILURE);
	}
	free(lockname);

	if (lockfd <= 2) {
		newfd = fcntl(lockfd, F_DUPFD, 3);
		if (newfd < 0) {
			pseudo_diag("Can't move lockfile to safe descriptor: %s\n",
				strerror(errno));
		} else {
			close(lockfd);
			lockfd = newfd;
		}
	}

	pseudo_debug(3, "acquiring lock.\n");
	if (flock(lockfd, LOCK_EX | LOCK_NB) < 0) {
		if (errno == EACCES || errno == EAGAIN) {
			pseudo_debug(1, "Existing server has lock.  Exiting.\n");
		} else {
			pseudo_diag("Error obtaining lock: %s\n", strerror(errno));
		}
		exit(0);
	} else {
		pseudo_debug(2, "Acquired lock.\n");
	}
	pseudo_debug(3, "serving (%s)\n", opt_d ? "daemon" : "foreground");
	return pseudo_server_start(opt_d);
}

/* 
 * actually process operations.
 * This first evaluates the message, figures out what's in the DB, does some
 * sanity checks, then implements the fairly small DB changes required.
 */
int
pseudo_op(pseudo_msg_t *msg, const char *program, const char *tag) {
	pseudo_msg_t msg_header;
	pseudo_msg_t by_path = { .op = 0 }, by_ino = { .op = 0 };
	pseudo_msg_t db_header;
	char *path_by_ino = 0;
	char *oldpath = 0;
	int found_path = 0, found_ino = 0;
	int prefer_ino = 0;

	if (!msg)
		return 1;
	
	msg->result = RESULT_SUCCEED;

	/* debugging message.  Primary key first. */
	switch (msg->op) {
	case OP_FCHOWN:		/* FALLTHROUGH */
	case OP_FCHMOD:		/* FALLTHROUGH */
	case OP_FSTAT:
		prefer_ino = 1;
		pseudo_debug(2, "%s %llu [%s]: ", pseudo_op_name(msg->op),
			(unsigned long long) msg->ino,
			msg->pathlen ? msg->path : "no path");
		break;
	default:
		pseudo_debug(2, "%s %s [%llu]: ", pseudo_op_name(msg->op),
			msg->pathlen ? msg->path : "no path",
			(unsigned long long) msg->ino);
		break;
	}

	/* stash original header, in case we need it later */
	msg_header = *msg;

	/* There should usually be a path.  Even for f* ops, the client
	 * tries to provide a path from its table of known fd paths.
	 */
	if (msg->pathlen) {
		if (msg->op == OP_RENAME) {
			oldpath = msg->path + strlen(msg->path) + 1;
			pseudo_debug(2, "rename: path %s, oldpath %s\n",
				msg->path, oldpath);
		}
		/* for now, don't canonicalize paths anymore */
		/* used to do it here, but now doing it in client */
		if (!pdb_find_file_path(msg)) {
			by_path = *msg;
			found_path = 1;
		} else {
			if (msg->op != OP_RENAME && msg->op != OP_LINK) {
				pseudo_debug(3, "(new?) ");
			}
		}
	}

	/* search on original inode -- in case of mismatch */
	by_ino = msg_header;
	if (msg_header.ino != 0) {
		if (msg->pathlen && !pdb_find_file_exact(msg)) {
			/* restore header contents */
			by_ino = *msg;
			*msg = msg_header;
			found_ino = 1;
			/* note:  we have to avoid freeing this later */
			path_by_ino = msg->path;
		} else if (!pdb_find_file_dev(&by_ino)) {
			found_ino = 1;
			path_by_ino = pdb_get_file_path(&by_ino);
		}
	}

	pseudo_debug(3, "incoming: '%s'%s [%llu]%s\n",
		msg->pathlen ? msg->path : "no path",
		found_path ? "+" : "-",
		(unsigned long long) msg_header.ino,
		found_ino ? "+" : "-");

	if (found_path) {
		/* This is a bad sign.  We should never have a different entry
		 * for the inode...  But an inode of 0 from an EXEC is normal,
		 * we don't track those.
		 */
		if (by_path.ino != msg_header.ino && msg_header.ino != 0) {
			switch (msg->op) {
			case OP_EXEC:	/* FALLTHROUGH */
			case OP_RENAME:
				/* A rename that crossed a filesystem can change the inode
				 * number legitimately.
				 */
				pseudo_debug(2, "inode changed for '%s': %llu in db, %llu in request.\n",
					msg->path,
					(unsigned long long) by_path.ino,
					(unsigned long long) msg_header.ino);
				break;
			default:
				pseudo_diag("inode mismatch: '%s' ino %llu in db, %llu in request.\n",
					msg->path,
					(unsigned long long) by_path.ino,
					(unsigned long long) msg_header.ino);
			}
		}
		/* If the database entry disagrees on S_ISDIR, it's just
		 * plain wrong.  We remove the database entry, because it
		 * is absolutely certain to be wrong.  This means found_path
		 * is now 0, because there is no entry in db...
		 *
		 * This used to unlink everything with the inode from
		 * the message -- but what if the entry in the database
		 * had a different inode?  We should nuke THAT inode,
		 * and everything agreeing with it, which will also catch
		 * the bogus entry that we noticed.
		 */
		if (S_ISDIR(by_path.mode) != S_ISDIR(msg_header.mode)) {
			pseudo_diag("dir mismatch: '%s' [%llu] db mode 0%o, header mode 0%o (unlinking db)\n",
				msg->path, (unsigned long long) by_path.ino,
				(int) by_path.mode, (int) msg_header.mode);
			/* unlink everything with this inode */
			pdb_unlink_file_dev(&by_path);
			found_path = 0;
		} else if (S_ISLNK(by_path.mode) != S_ISLNK(msg_header.mode)) {
			pseudo_diag("symlink mismatch: '%s' [%llu] db mode 0%o, header mode 0%o (unlinking db)\n",
				msg->path, (unsigned long long) by_path.ino,
				(int) by_path.mode, (int) msg_header.mode);
			/* unlink everything with this inode */
			pdb_unlink_file_dev(&by_path);
			found_path = 0;
		}
	}

	if (found_ino) {
		/* Not always an absolute failure case.
		 * If a file descriptor shows up unexpectedly and gets
		 * fchown()d, you could have an entry giving the inode and
		 * data, but not path.  So, we add the path to the entry.
		 * Any other changes from the incoming message will be applied
		 * at leisure.
		 */
		if (msg->pathlen && !path_by_ino) {
			pseudo_debug(2, "db path missing: ino %llu, request '%s'.\n",
				(unsigned long long) msg_header.ino, msg->path);
			pdb_update_file_path(msg);
		} else if (!msg->pathlen && path_by_ino) {
			/* harmless */
			pseudo_debug(2, "req path missing: ino %llu, db '%s'.\n",
				(unsigned long long) msg_header.ino, path_by_ino);
		} else if (msg->pathlen && path_by_ino) {
			/* this suggests a database error, except in LINK
			 * cases.  In those cases, it is normal for a
			 * mismatch to occur.  :)  (SYMLINK shouldn't,
			 * because the symlink gets its own inode number.)
			 *
			 * RENAME can get false positives on this, when
			 * link count is greater than one.  So we skip this
			 * test for OP_LINK (always) and OP_RENAME (for link
			 * count greater than one).  For RENAME, the test
			 * should be against the old name, though!
			 */
			int mismatch = 0;
			switch (msg->op) {
			case OP_LINK:
			case OP_EXEC:
				break;
			case OP_RENAME:
				if (msg->nlink == 1 && strcmp(oldpath, path_by_ino)) {
					mismatch = 1;
				}
				break;
			default:
				if (strcmp(msg->path, path_by_ino)) {
					mismatch = 1;
				}
				break;
			}
			if (mismatch) {
				pseudo_diag("path mismatch [%d link%s]: ino %llu db '%s' req '%s'.\n",
					msg->nlink,
					msg->nlink == 1 ? "" : "s",
					(unsigned long long) msg_header.ino,
					path_by_ino ? path_by_ino : "no path",
					msg->path);
			}
		} else {
			/* I don't think I've ever seen this one. */
			pseudo_debug(1, "warning: ino %llu in db (mode 0%o, owner %d), no path known.\n",
				(unsigned long long) msg_header.ino,
				(int) by_ino.mode, (int) by_ino.uid);
		}
		/* Again, in the case of a directory mismatch, nuke the DB
		 * entry.  There is no way it can be right.
		 */
		if (S_ISDIR(by_ino.mode) != S_ISDIR(msg_header.mode)) {
			pseudo_diag("dir err : %llu ['%s'] (db '%s') db mode 0%o, header mode 0%o (unlinking db)\n",
				(unsigned long long) msg_header.ino,
				msg->pathlen ? msg->path : "no path",
				path_by_ino ? path_by_ino : "no path",
				(int) by_ino.mode, (int) msg_header.mode);
			pdb_unlink_file_dev(msg);
			found_ino = 0;
		} else if (S_ISLNK(by_ino.mode) != S_ISLNK(msg_header.mode)) {
			/* In the current implementation, only msg_header.mode
			 * can ever be a symlink; the test is generic as
			 * insurance against forgetting to fix it in a future
			 * update. */
			pseudo_diag("symlink err : %llu ['%s'] (db '%s') db mode 0%o, header mode 0%o (unlinking db)\n",
				(unsigned long long) msg_header.ino,
				msg->pathlen ? msg->path : "no path",
				path_by_ino ? path_by_ino : "no path",
				(int) by_ino.mode, (int) msg_header.mode);
			pdb_unlink_file_dev(msg);
			found_ino = 0;
		}
	}

	/* In the case of a stat() call, if a mismatch existed, we prefer
	 * by-inode for fstat, by-path for stat.  Nothing else actually uses
	 * this...
	 */
	if (found_ino && (prefer_ino || !found_path)) {
		db_header = by_ino;
	} else if (found_path) {
		db_header = by_path;
	}

	switch (msg->op) {
	case OP_CHDIR:		/* FALLTHROUGH */
	case OP_CLOSE:
		/* these messages are handled entirely on the client side,
		 * as of this writing, but might be logged by accident: */
		pseudo_diag("error: op %s sent to server.\n", pseudo_op_name(msg->op));
		break;
	case OP_EXEC:		/* FALLTHROUGH */
	case OP_OPEN:
		/* nothing to do -- just sent in case we're logging */
		break;
	case OP_CREAT:
		/* implies a new file -- not a link, which would be OP_LINK */
		if (found_ino) {
			/* CREAT should never be sent if the file existed.
			 * So, any existing entry is an error.  Nuke it.
			 */
			pseudo_diag("creat for '%s' replaces existing %llu ['%s'].\n",
				msg->pathlen ? msg->path : "no path",
				(unsigned long long) by_ino.ino,
				path_by_ino ? path_by_ino : "no path");
			pdb_unlink_file_dev(&by_ino);
		}
		if (!found_path) {
			pdb_link_file(msg);
		} else {
			/* again, an error, but leaving it alone for now. */
			pseudo_diag("creat ignored for existing file '%s'.\n",
				msg->pathlen ? msg->path : "no path");
		}
		break;
	case OP_CHMOD:		/* FALLTHROUGH */
	case OP_FCHMOD:
		pseudo_debug(2, "mode 0%o ", (int) msg->mode);
		/* if the inode is known, update it */
		if (found_ino) {
			/* obtain the existing data, merge with mode */
			*msg = by_ino;
			msg->mode = (msg_header.mode & 07777) |
				    (msg->mode & ~07777);
			pdb_update_file(msg);
		} else if (found_path) {
			/* obtain the existing data, merge with mode */
			*msg = by_path;
			msg->mode = (msg_header.mode & 07777) |
				    (by_path.mode & ~07777);
			pdb_update_file(msg);
		} else {
			/* just in case find_file_path screwed up the msg */
			msg->mode = msg_header.mode;
		}
		/* if the path is not known, link it */
		if (!found_path) {
			pseudo_debug(2, "(new) ");
			pdb_link_file(msg);
		}
		break;
	case OP_CHOWN:		/* FALLTHROUGH */
	case OP_FCHOWN:
		pseudo_debug(2, "owner %d:%d ", (int) msg_header.uid, (int) msg_header.gid);
		/* if the inode is known, update it */
		if (found_ino) {
			/* obtain the existing data, merge with mode */
			*msg = by_ino;
			msg->uid = msg_header.uid;
			msg->gid = msg_header.gid;
			pdb_update_file(msg);
		} else if (found_path) {
			/* obtain the existing data, merge with mode */
			*msg = by_path;
			msg->uid = msg_header.uid;
			msg->gid = msg_header.gid;
			pdb_update_file(msg);
		} else {
			/* just in case find_file_path screwed up the msg */
			msg->uid = msg_header.uid;
			msg->gid = msg_header.gid;
		}
		/* if the path is not known, link it */
		if (!found_path) {
			pdb_link_file(msg);
		}
		break;
	case OP_STAT:		/* FALLTHROUGH */
	case OP_FSTAT:
		/* db_header will be whichever one looked best, in the rare
		 * case where there might be a clash.
		 */
		if (found_ino || found_path) {
			*msg = db_header;
		} else {
			msg->result = RESULT_FAIL;
		}
		pseudo_debug(3, "%s, ino %llu (old mode 0%o): mode 0%o\n",
			pseudo_op_name(msg->op), (unsigned long long) msg->ino,
			(int) msg_header.mode, (int) msg->mode);
		break;
	case OP_LINK:		/* FALLTHROUGH */
	case OP_SYMLINK:
		/* a successful link (client only notifies us for those)
		 * implies that the new path did not previously exist, and
		 * the old path did.  We get the stat buffer and the new path.
		 * So, we unlink it, then link it.  Neither unlink nor link
		 * touches the message, which was initialized from the
		 * underlying file data in the client.
		 */
		if (found_path) {
			pseudo_debug(2, "replace %slink: path %s, old ino %llu, mode 0%o, new ino %llu, mode 0%o\n",
				msg->op == OP_SYMLINK ? "sym" : "",
				msg->path, (unsigned long long) msg->ino,
				(int) msg->mode,
				(unsigned long long) msg_header.ino,
				(int) msg_header.mode);
			pdb_unlink_file(msg);
		} else {
			pseudo_debug(2, "new %slink: path %s, ino %llu, mode 0%o\n",
				msg->op == OP_SYMLINK ? "sym" : "",
				msg->path,
				(unsigned long long) msg_header.ino,
				(int) msg_header.mode);
		}
		if (found_ino) {
			if (msg->op == OP_SYMLINK) {
				pseudo_debug(2, "symlink: ignoring existing file %llu ['%s']\n",
					(unsigned long long) by_ino.ino,
					path_by_ino ? path_by_ino : "no path");
			} else {
				*msg = by_ino;
				pseudo_debug(2, "link: copying data from existing file %llu ['%s']\n",
					(unsigned long long) by_ino.ino,
					path_by_ino ? path_by_ino : "no path");
			}
		} else {
			*msg = msg_header;
		}
		pdb_link_file(msg);
		break;
	case OP_RENAME:
		/* a rename implies renaming an existing entry... and every
		 * database entry rooted in it, if it's a directory.
		 */
		pdb_rename_file(oldpath, msg);
		pdb_update_inode(msg);
		break;
	case OP_UNLINK:
		/* this removes any entries with the given path from the
		 * database.  No response is needed.
		 * DO NOT try to fail if the entry is already gone -- if the
		 * server's response didn't make it, the client would resend.
		 */
		pdb_unlink_file(msg);
		pdb_unlink_contents(msg);
		/* If we are seeing an unlink for something with only one
		 * link, we should delete all records for that inode, even
		 * ones through different paths.  This handles the case
		 * where something is removed through the wrong path, but
		 * only if it didn't have multiple hard links.
		 *
		 * This should cease to be needed once symlinks are tracked.
		 */
		if (msg_header.nlink == 1 && found_ino) {
			pseudo_debug(2, "link count 1, unlinking anything with ino %llu.\n",
				(unsigned long long) msg->ino);
			pdb_unlink_file_dev(msg);
		}
		/* if we had a match for this path, stash it in the
		 * message -- client may want to relink it if the
		 * real_unlink() fails.
		 */
		if (found_path) {
			*msg = by_path;
			msg->result = RESULT_SUCCEED;
		} else {
			msg->result = RESULT_NONE;
		}
		break;
	case OP_MKDIR:		/* FALLTHROUGH */
	case OP_MKNOD:
		pseudo_debug(2, "mode 0%o", (int) msg->mode);
		/* for us to get called, the client has to have succeeded in
		 * a creation (of a regular file, for mknod) -- meaning this
		 * file DID NOT exist before the call.  Fix database:
		 */
		if (found_path) {
			pseudo_diag("mkdir/mknod: '%s' [%llu] already existed (mode 0%o), unlinking\n",
				msg->path, (unsigned long long) by_path.ino,
				(int) by_path.mode);
			pdb_unlink_file(msg);
		}
		if (found_ino) {
			pdb_unlink_file_dev(&by_ino);
		}
		*msg = msg_header;
		pdb_link_file(msg);
		break;
	default:
		pseudo_diag("unknown op from client %d, op %d [%s]\n",
			msg->client, msg->op,
			msg->pathlen ? msg->path : "no path");
		break;
	}
	/* in the case of an exact match, we just used the pointer
	 * rather than allocating space
	 */
	if (path_by_ino != msg->path)
		free(path_by_ino);
	pseudo_debug(2, "completed %s.\n", pseudo_op_name(msg->op));
	if (opt_l)
		pdb_log_msg(SEVERITY_INFO, msg, program, tag, NULL);
	return 0;
}

/* SHUTDOWN does not get this far, it's handled in pseudo_server.c */
int
pseudo_server_response(pseudo_msg_t *msg, const char *program, const char *tag) {
	switch (msg->type) {
	case PSEUDO_MSG_PING:
		msg->result = RESULT_SUCCEED;
		if (opt_l)
			pdb_log_msg(SEVERITY_INFO, msg, program, tag, NULL);
		return 0;
		break;
	case PSEUDO_MSG_OP:
		return pseudo_op(msg, program, tag);
		break;
	case PSEUDO_MSG_ACK:		/* FALLTHROUGH */
	case PSEUDO_MSG_NAK:		/* FALLTHROUGH */
	default:
		pdb_log_msg(SEVERITY_WARN, msg, program, tag, "invalid message");
		return 1;
	}
}

int
pseudo_db_check(void) {
	struct stat64 buf;
	pseudo_msg_t *m;
	pdb_file_list l;
	int errors = 0;

	l = pdb_files();
	if (!l) {
		pseudo_diag("Couldn't start file list, can't scan.\n");
		return EXIT_FAILURE;
	}
	while ((m = pdb_file(l)) != NULL) {
		pseudo_debug(2, "m: %p (%d: %s)\n",
			(void *) m,
			m ? (int) m->pathlen : -1,
			m ? m->path : "<n/a>");
		if (m->pathlen > 0) {
			pseudo_debug(1, "Checking <%s>\n", m->path);
			if (lstat64(m->path, &buf)) {
				errors = EXIT_FAILURE;
				pseudo_diag("can't stat <%s>\n", m->path);
				continue;
			}
			if (buf.st_ino != m->ino) {
				pseudo_diag("ino mismatch <%s>: ino %llu, db %llu\n",
					m->path,
					(unsigned long long) buf.st_ino,
					(unsigned long long) m->ino);
				errors = EXIT_FAILURE;
			}
			if (buf.st_dev != m->dev) {
				pseudo_diag("dev mismatch <%s>: dev %llu, db %llu\n",
					m->path,
					(unsigned long long) buf.st_dev,
					(unsigned long long) m->dev);
				errors = EXIT_FAILURE;
			}
			if (S_ISLNK(buf.st_mode) != S_ISLNK(m->mode)) {
				pseudo_diag("symlink mismatch <%s>: file %d, db %d\n",
					m->path,
					S_ISLNK(buf.st_mode),
					S_ISLNK(m->mode));
				errors = EXIT_FAILURE;
			}
			if (S_ISDIR(buf.st_mode) != S_ISDIR(m->mode)) {
				pseudo_diag("symlink mismatch <%s>: file %d, db %d\n",
					m->path,
					S_ISDIR(buf.st_mode),
					S_ISDIR(m->mode));
				errors = EXIT_FAILURE;
			}
			/* can't check for device type mismatches, uid/gid, or
			 * permissions, because those are the very things we
			 * can't really set.
			 */
		}
	}
	pdb_files_done(l);
	return errors;
}
