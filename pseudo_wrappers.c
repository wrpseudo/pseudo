/* wrapper code -- this is the shared code used around the pseduo
 * wrapper functions, which are in pseudo_wrapfuncs.c.
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

/* the generated code goes here */
#include "pseudo_wrapper_table.c"
#include "pseudo_wrapfuncs.c"

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
			if (*pseudo_functions[i].real == NULL) {
				int (*f)(void);
				char *e;
				dlerror();
				f = dlsym(RTLD_NEXT, pseudo_functions[i].name);
				if ((e = dlerror()) != NULL) {
					/* leave it NULL, which our implementation checks for */
					pseudo_diag("No wrapper for %s: %s\n", pseudo_functions[i].name, e);
				} else {
					if (f)
						*pseudo_functions[i].real = f;
				}
			}
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

static char **
execl_to_v(va_list ap, const char *argv0, char *const **envp) {
	size_t i = 0;
	size_t alloc_size = 256;

	char **argv = malloc((sizeof *argv) * alloc_size);

	if (!argv) {
		pseudo_debug(1, "execl failed: couldn't allocate memory for %lu arguments\n",
			(unsigned long) alloc_size);
		return NULL;
	}
	argv[i++] = (char *) argv0;

	while (argv[i-1]) {
		argv[i++] = va_arg(ap, char *const);
		if (i > alloc_size - 1) {
			alloc_size = alloc_size + 256;
			argv = realloc(argv, (sizeof *argv) * alloc_size);
			if (!argv) {
				pseudo_debug(1, "execl failed: couldn't allocate memory for %lu arguments\n",
					(unsigned long) alloc_size);
				return NULL;
			}
		}
	}
	if (envp) {
		*envp = va_arg(ap, char **);
	}
	return argv;
}

/* The following wrappers require Special Handling */

int
execl(const char *file, const char *arg, ...) {
	sigset_t saved;
	va_list ap;
	char **argv;

	int rc = -1;

	if (!pseudo_check_wrappers()) {
		/* rc was initialized to the "failure" value */
		pseudo_enosys("execl");
		return rc;
	}

	va_start(ap, arg);
	argv = execl_to_v(ap, arg, 0);
	va_end(ap);
	if (!argv) {
		errno = ENOMEM;
		return -1;
	}

	pseudo_debug(4, "called: execl\n");
	pseudo_sigblock(&saved);
	if (pseudo_getlock()) {
		errno = EBUSY;
		sigprocmask(SIG_SETMASK, &saved, NULL);
		return -1;
	}

	int save_errno;

	/* exec*() use this to restore the sig mask */
	pseudo_saved_sigmask = saved;
	rc = wrap_execv(file, argv);

	save_errno = errno;
	pseudo_droplock();
	sigprocmask(SIG_SETMASK, &saved, NULL);
	pseudo_debug(4, "completed: execl\n");
	errno = save_errno;
	free(argv);
	return rc;
}

int
execlp(const char *file, const char *arg, ...) {
	sigset_t saved;
	va_list ap;
	char **argv;

	int rc = -1;

	if (!pseudo_check_wrappers()) {
		/* rc was initialized to the "failure" value */
		pseudo_enosys("execlp");
		return rc;
	}

	va_start(ap, arg);
	argv = execl_to_v(ap, arg, 0);
	va_end(ap);
	if (!argv) {
		errno = ENOMEM;
		return -1;
	}

	pseudo_debug(4, "called: execlp\n");
	pseudo_sigblock(&saved);
	if (pseudo_getlock()) {
		errno = EBUSY;
		sigprocmask(SIG_SETMASK, &saved, NULL);
		return -1;
	}

	int save_errno;

	/* exec*() use this to restore the sig mask */
	pseudo_saved_sigmask = saved;
	rc = wrap_execvp(file, argv);

	save_errno = errno;
	pseudo_droplock();
	sigprocmask(SIG_SETMASK, &saved, NULL);
	pseudo_debug(4, "completed: execlp\n");
	errno = save_errno;
	free(argv);
	return rc;
}

int
execle(const char *file, const char *arg, ...) {
	sigset_t saved;
	va_list ap;
	char **argv;
	char **envp;

	int rc = -1;

	if (!pseudo_check_wrappers()) {
		/* rc was initialized to the "failure" value */
		pseudo_enosys("execle");
		return rc;
	}

	va_start(ap, arg);
	argv = execl_to_v(ap, arg, (char *const **)&envp);
	va_end(ap);
	if (!argv) {
		errno = ENOMEM;
		return -1;
	}

	pseudo_debug(4, "called: execle\n");
	pseudo_sigblock(&saved);
	if (pseudo_getlock()) {
		errno = EBUSY;
		sigprocmask(SIG_SETMASK, &saved, NULL);
		return -1;
	}

	int save_errno;

	/* exec*() use this to restore the sig mask */
	pseudo_saved_sigmask = saved;
	rc = wrap_execve(file, argv, envp);

	save_errno = errno;
	pseudo_droplock();
	sigprocmask(SIG_SETMASK, &saved, NULL);
	pseudo_debug(4, "completed: execle\n");
	errno = save_errno;
	free(argv);
	return rc;
}

int
execv(const char *file, char *const *argv) {
	sigset_t saved;
	
	int rc = -1;

	if (!pseudo_check_wrappers() || !real_execv) {
		/* rc was initialized to the "failure" value */
		pseudo_enosys("execv");
		return rc;
	}

	pseudo_debug(4, "called: execv\n");
	pseudo_sigblock(&saved);
	if (pseudo_getlock()) {
		errno = EBUSY;
		sigprocmask(SIG_SETMASK, &saved, NULL);
		return -1;
	}

	int save_errno;
			
	/* exec*() use this to restore the sig mask */
	pseudo_saved_sigmask = saved;
	rc = wrap_execv(file, argv);
		
	save_errno = errno;
	pseudo_droplock();
	sigprocmask(SIG_SETMASK, &saved, NULL);
	pseudo_debug(4, "completed: execv\n");
	errno = save_errno;
	return rc;
}

int
execve(const char *file, char *const *argv, char *const *envp) {
	sigset_t saved;
	
	int rc = -1;

	if (!pseudo_check_wrappers() || !real_execve) {
		/* rc was initialized to the "failure" value */
		pseudo_enosys("execve");
		return rc;
	}

	pseudo_debug(4, "called: execve\n");
	pseudo_sigblock(&saved);
	if (pseudo_getlock()) {
		errno = EBUSY;
		sigprocmask(SIG_SETMASK, &saved, NULL);
		return -1;
	}

	int save_errno;
			
	/* exec*() use this to restore the sig mask */
	pseudo_saved_sigmask = saved;
	rc = wrap_execve(file, argv, envp);
		
	save_errno = errno;
	pseudo_droplock();
	sigprocmask(SIG_SETMASK, &saved, NULL);
	pseudo_debug(4, "completed: execve\n");
	errno = save_errno;
	return rc;
}

int
execvp(const char *file, char *const *argv) {
	sigset_t saved;
	
	int rc = -1;

	if (!pseudo_check_wrappers() || !real_execvp) {
		/* rc was initialized to the "failure" value */
		pseudo_enosys("execvp");
		return rc;
	}

	pseudo_debug(4, "called: execvp\n");
	pseudo_sigblock(&saved);
	if (pseudo_getlock()) {
		errno = EBUSY;
		sigprocmask(SIG_SETMASK, &saved, NULL);
		return -1;
	}

	int save_errno;
			
	/* exec*() use this to restore the sig mask */
	pseudo_saved_sigmask = saved;
	rc = wrap_execvp(file, argv);
		
	save_errno = errno;
	pseudo_droplock();
	sigprocmask(SIG_SETMASK, &saved, NULL);
	pseudo_debug(4, "completed: execvp\n");
	errno = save_errno;
	return rc;
}

int
fork(void) {
	sigset_t saved;
	
	int rc = -1;

	if (!pseudo_check_wrappers() || !real_fork) {
		/* rc was initialized to the "failure" value */
		pseudo_enosys("fork");
		return rc;
	}

	pseudo_debug(4, "called: fork\n");
	pseudo_sigblock(&saved);
	if (pseudo_getlock()) {
		errno = EBUSY;
		sigprocmask(SIG_SETMASK, &saved, NULL);
		return -1;
	}

	int save_errno;
			
	rc = wrap_fork();

	save_errno = errno;
		
	pseudo_droplock();
	sigprocmask(SIG_SETMASK, &saved, NULL);
	pseudo_debug(4, "completed: fork\n");
	errno = save_errno;
	return rc;
}

int
vfork(void) {
	/* we don't provide support for the distinct semantics
	 * of vfork()
	 */
	return fork();
}

int
clone(int (*fn)(void *), void *child_stack, int flags, void *arg, ...) {
	sigset_t saved;
	va_list ap;
	pid_t *pid;
	struct user_desc *tls;
	pid_t *ctid;

	int rc = -1;

	if (!pseudo_check_wrappers() || !real_clone) {
		/* rc was initialized to the "failure" value */
		pseudo_enosys("clone");
		return rc;
	}

	va_start(ap, arg);
	pid = va_arg(ap, pid_t *);
	tls = va_arg(ap, struct user_desc *);
	ctid = va_arg(ap, pid_t *);
	va_end(ap);

	pseudo_debug(4, "called: clone\n");
	pseudo_sigblock(&saved);
	if (pseudo_getlock()) {
		errno = EBUSY;
		sigprocmask(SIG_SETMASK, &saved, NULL);
		return -1;
	}

	int save_errno;
	int save_disabled = pseudo_disabled;
	/* because clone() doesn't actually continue in this function, we
	 * can't check the return and fix up environment variables in the
	 * child.  Instead, we have to temporarily do any fixup, then possibly
	 * undo it later.  UGH!
	 */

#include "guts/clone.c"

	if (save_disabled != pseudo_disabled) {
		if (pseudo_disabled) {
			pseudo_disabled = 0;
			pseudo_magic();
		} else {
			pseudo_disabled = 1;
			pseudo_antimagic();
		}
	}
		
	save_errno = errno;
	pseudo_droplock();
	sigprocmask(SIG_SETMASK, &saved, NULL);
	pseudo_debug(4, "completed: clone\n");
	errno = save_errno;
	return rc;
}

#if 0
static int (*real_execlp)(const char *file, const char *arg, ...) = NULL;
static int (*real_execl)(const char *file, const char *arg, ...) = NULL;
static int (*real_execle)(const char *file, const char *arg, ...) = NULL;
#endif
static int (*real_execv)(const char *file, char *const *argv) = NULL;
static int (*real_execve)(const char *file, char *const *argv, char *const *envp) = NULL;
static int (*real_execvp)(const char *file, char *const *argv) = NULL;
static int (*real_fork)(void) = NULL;
static int (*real_clone)(int (*)(void *), void *, int, void *, ...) = NULL;

static int
wrap_execv(const char *file, char *const *argv) {
	int rc = -1;

#include "guts/execv.c"

	return rc;
}

static int
wrap_execve(const char *file, char *const *argv, char *const *envp) {
	int rc = -1;

#include "guts/execve.c"

	return rc;
}

static int
wrap_execvp(const char *file, char *const *argv) {
	int rc = -1;

#include "guts/execvp.c"

	return rc;
}

static int
wrap_fork(void) {
	int rc = -1;
	
#include "guts/fork.c"

	return rc;
}

int
wrap_clone(int (*fn)(void *), void *child_stack, int flags, void *arg, ...) {
	/* unused */
	return 0;
}
