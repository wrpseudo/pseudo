/*
 * pseudo_wrappers.c, shared code for wrapper functions
 *
 * Copyright (c) 2008-2012 Wind River Systems, Inc.
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
#include <assert.h>
#include <stdlib.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>

#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <dlfcn.h>

/* used for various specific function arguments */
#include <dirent.h>
#include <fts.h>
#include <ftw.h>
#include <glob.h>
#include <grp.h>
#include <pwd.h>
#include <utime.h>

#include "pseudo.h"
#include "pseudo_wrapfuncs.h"
#include "pseudo_ipc.h"
#include "pseudo_client.h"

/* Types and declarations we need in advance. */
#include "pseudo_wrapper_table.c"

static void pseudo_enosys(const char *);
static int pseudo_check_wrappers(void);
static volatile int antimagic = 0;
static pthread_mutex_t pseudo_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_t pseudo_mutex_holder;
static int pseudo_mutex_recursion = 0;
static int pseudo_getlock(void);
static void pseudo_droplock(void);
static size_t pseudo_dechroot(char *, size_t);
static void pseudo_sigblock(sigset_t *);

extern char *program_invocation_short_name;
static sigset_t pseudo_saved_sigmask;

/* Constructor only exists in libpseudo */
static void _libpseudo_init(void) __attribute__ ((constructor));

static int _libpseudo_initted = 0;

static void
_libpseudo_init(void) {
	pseudo_getlock();
	pseudo_antimagic();
	_libpseudo_initted = 1;

	pseudo_init_util();
	pseudo_init_wrappers();
	pseudo_init_client();

	pseudo_magic();
	pseudo_droplock();
}

void
pseudo_reinit_libpseudo(void) {
	_libpseudo_init();
}

static void
pseudo_init_one_wrapper(pseudo_function *func) {
	int (*f)(void) = (int (*)(void)) NULL;

	char *e;
	if (*func->real != NULL) {
		/* already initialized */
		return;
	}
	dlerror();

#if PSEUDO_PORT_LINUX
	if (func->version)
		f = dlvsym(RTLD_NEXT, func->name, func->version);
	/* fall through to the general case, if that failed */
	if (!f)
#endif
	f = dlsym(RTLD_NEXT, func->name);
	if (f) {
		*func->real = f;
	} else {
		e = dlerror();
#ifdef PSEUDO_NO_REAL_AT_FUNCTIONS
		char *s = func->name;
		s += strlen(s) - 2;
		/* *at() don't have to exist */
		if (!strcmp(s, "at")) {
			return;
		}
#else
		if (e != NULL) {
			pseudo_diag("No real function for %s: %s\n", func->name, e);
		}
#endif
	}
}

void
pseudo_init_wrappers(void) {
	int i;
	static int done = 0;

	pseudo_getlock();
	pseudo_antimagic();

	/* We only ever want to run this once, even though we might want to
	 * "re-init" at specific times...
	 */
	if (!done) {
		for (i = 0; pseudo_functions[i].name; ++i) {
			pseudo_init_one_wrapper(&pseudo_functions[i]);
		}
		done = 1;
	}

	/* Once the wrappers are setup, we can now use open... so
	 * setup the logfile, if necessary...
	 */
	pseudo_logfile(NULL);

	pseudo_magic();
	pseudo_droplock();
}

static void
pseudo_sigblock(sigset_t *saved) {
	sigset_t blocked;

	/* these are signals for which the handlers often
	 * invoke operations, such as close(), which are handled
	 * by pseudo and could result in a deadlock.
	 */
	sigemptyset(&blocked);
	sigaddset(&blocked, SIGALRM);	/* every-N-seconds tasks */
	sigaddset(&blocked, SIGCHLD);	/* reaping child processes */
	sigaddset(&blocked, SIGHUP);	/* idiomatically, reloading config */
	sigaddset(&blocked, SIGTERM);	/* shutdown/teardown operations */
	sigaddset(&blocked, SIGUSR1);	/* reopening log files, sometimes */
	sigaddset(&blocked, SIGUSR2);	/* who knows what people do */
	sigprocmask(SIG_BLOCK, &blocked, saved);
}

static int
pseudo_getlock(void) {
	if (pthread_equal(pseudo_mutex_holder, pthread_self())) {
		++pseudo_mutex_recursion;
		return 0;
	} else {
		if (pthread_mutex_lock(&pseudo_mutex) == 0) {
			pseudo_mutex_recursion = 1;
			pseudo_mutex_holder = pthread_self();
			return 0;
		} else {
			return -1;
		}
	}
}

static void
pseudo_droplock(void) {
	if (--pseudo_mutex_recursion == 0) {
		pseudo_mutex_holder = 0;
		pthread_mutex_unlock(&pseudo_mutex);
	}
}

void
pseudo_antimagic() {
	++antimagic;
}

void
pseudo_magic() {
	if (antimagic > 0)
		--antimagic;
}

static void
pseudo_enosys(const char *func) {
	pseudo_diag("pseudo: ENOSYS for '%s'.\n", func ? func : "<nil>");
	char * value = pseudo_get_value("PSEUDO_ENOSYS_ABORT");
	if (value)
		abort();
	free(value);
	errno = ENOSYS;
}

/* de-chroot a string.
 * note that readlink() yields an unterminated buffer, so
 * must pass in the correct length.  Buffers are null-terminated
 * unconditionally if they are modified -- the modification would
 * shorten the string, so there will be space for the NUL, so
 * this is safe even for stuff like readlink().
 */
static size_t
pseudo_dechroot(char *s, size_t len) {
	if (len == (size_t) -1)
		len = strlen(s);
	if (pseudo_chroot_len && len >= pseudo_chroot_len &&
		!memcmp(s, pseudo_chroot, pseudo_chroot_len)) {
		if (s[pseudo_chroot_len] == '/') {
			memmove(s, s + pseudo_chroot_len, len - pseudo_chroot_len);
			len -= pseudo_chroot_len;
			s[len] = '\0';
		} else if (s[pseudo_chroot_len] == '\0') {
			s[0] = '/';
			len = 1;
			s[len] = '\0';
		}
		/* otherwise, it's not really a match... */
	}
	return len;
}

static int
pseudo_check_wrappers(void) {
	if (!_libpseudo_initted)
		pseudo_reinit_libpseudo();

	return _libpseudo_initted;
}		

/* the generated code goes here */
#include "port_wrappers.c"
#include "pseudo_wrapfuncs.c"

