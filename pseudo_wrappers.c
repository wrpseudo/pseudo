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
static int pseudo_populate_wrappers(void);
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
pseudo_populate_wrappers(void) {
	int i;
	char *debug;
	static int done = 0;
	char *pseudo_path = 0;
	char *no_symlink_exp;

	if (done)
		return done;
	pseudo_getlock();
	pseudo_antimagic();
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
	debug = pseudo_get_value("PSEUDO_DEBUG");
	if (debug) {
		int level = atoi(debug);
		for (i = 0; i < level; ++i) {
			pseudo_debug_verbose();
		}
	}
	free(debug);
	no_symlink_exp = pseudo_get_value("PSEUDO_NOSYMLINKEXP");
	if (no_symlink_exp) {
		char *endptr;
		/* if the environment variable is not an empty string,
		 * parse it; "0" means turn NOSYMLINKEXP off, "1" means
		 * turn it on (disabling the feature).  An empty string
		 * or something we can't parse means to set the flag; this
		 * is a safe default because if you didn't want the flag
		 * set, you normally wouldn't set the environment variable
		 * at all.
		 */
		if (*no_symlink_exp) {
			pseudo_nosymlinkexp = strtol(no_symlink_exp, &endptr, 10);
			if (*endptr)
				pseudo_nosymlinkexp = 1;
		} else {
			pseudo_nosymlinkexp = 1;
		}
	} else {
		pseudo_nosymlinkexp = 0;
	}
	free(no_symlink_exp);
	/* if PSEUDO_DEBUG_FILE is set up, redirect logging there.
	 */
	pseudo_logfile(NULL);
	/* must happen after wrappers are set up, because it can call
	 * getcwd(), which needs wrappers, but must happen here so that
	 * any attempt to use a path in a wrapper function will have a
	 * value for cwd.
	 */
	pseudo_client_reset();
	pseudo_path = pseudo_prefix_path(NULL);
	if (pseudo_prefix_dir_fd == -1) {
		if (pseudo_path) {
			pseudo_prefix_dir_fd = open(pseudo_path, O_RDONLY);
			pseudo_prefix_dir_fd = pseudo_fd(pseudo_prefix_dir_fd, MOVE_FD);
		} else {
			pseudo_diag("No prefix available to to find server.\n");
			exit(1);
		}
		if (pseudo_prefix_dir_fd == -1) {
			pseudo_diag("Can't open prefix path (%s) for server: %s\n",
				pseudo_path,
				strerror(errno));
			exit(1);
		}
	}
	free(pseudo_path);
	pseudo_path = pseudo_localstatedir_path(NULL);
	if (pseudo_localstate_dir_fd == -1) {
		if (pseudo_path) {
			pseudo_localstate_dir_fd = open(pseudo_path, O_RDONLY);
			pseudo_localstate_dir_fd = pseudo_fd(pseudo_localstate_dir_fd, MOVE_FD);
		} else {
			pseudo_diag("No prefix available to to find server.\n");
			exit(1);
		}
		if (pseudo_localstate_dir_fd == -1) {
			pseudo_diag("Can't open prefix path (%s) for server: %s\n",
				pseudo_path,
				strerror(errno));
			exit(1);
		}
	}
	free(pseudo_path);
	pseudo_debug(2, "(%s) set up wrappers\n", program_invocation_short_name);
	pseudo_magic();
	pseudo_droplock();
	return done;
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
	if (pseudo_populate_wrappers()) {
		int save_errno;
		if (antimagic > 0) {
			if (real_execv) {
				/* use execv to emulate */
				rc = (*real_execv)(file, argv);
			} else {
				/* rc was initialized to the "failure" value */
				pseudo_enosys("execl");
			}
		} else {
			
			/* exec*() use this to restore the sig mask */
			pseudo_saved_sigmask = saved;
			rc = wrap_execv(file, argv);
		}

		save_errno = errno;
		pseudo_droplock();
		sigprocmask(SIG_SETMASK, &saved, NULL);
		pseudo_debug(4, "completed: execl\n");
		errno = save_errno;
		free(argv);
		return rc;
	} else {
		pseudo_droplock();
		sigprocmask(SIG_SETMASK, &saved, NULL);
		pseudo_debug(4, "completed: execl\n");
		/* rc was initialized to the "failure" value */
		pseudo_enosys("execl");
		free(argv);
		return rc;
	}
}

int
execlp(const char *file, const char *arg, ...) {
	sigset_t saved;
	va_list ap;
	char **argv;

	int rc = -1;

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
	if (pseudo_populate_wrappers()) {
		int save_errno;
		if (antimagic > 0) {
			if (real_execvp) {
				/* use execv to emulate */
				rc = (*real_execvp)(file, argv);
			} else {
				/* rc was initialized to the "failure" value */
				pseudo_enosys("execlp");
			}
		} else {
			
			/* exec*() use this to restore the sig mask */
			pseudo_saved_sigmask = saved;
			rc = wrap_execvp(file, argv);
		}

		save_errno = errno;
		pseudo_droplock();
		sigprocmask(SIG_SETMASK, &saved, NULL);
		pseudo_debug(4, "completed: execlp\n");
		errno = save_errno;
		free(argv);
		return rc;
	} else {
		pseudo_droplock();
		sigprocmask(SIG_SETMASK, &saved, NULL);
		pseudo_debug(4, "completed: execlp\n");
		/* rc was initialized to the "failure" value */
		pseudo_enosys("execlp");
		free(argv);
		return rc;
	}
}

int
execle(const char *file, const char *arg, ...) {
	sigset_t saved;
	va_list ap;
	char **argv;
	char **envp;

	int rc = -1;

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
	if (pseudo_populate_wrappers()) {
		int save_errno;
		if (antimagic > 0) {
			if (real_execve) {
				/* use execve to emulate */
				rc = (*real_execve)(file, argv, envp);
			} else {
				/* rc was initialized to the "failure" value */
				pseudo_enosys("execl");
			}
		} else {
			
			/* exec*() use this to restore the sig mask */
			pseudo_saved_sigmask = saved;
			rc = wrap_execve(file, argv, envp);
		}

		save_errno = errno;
		pseudo_droplock();
		sigprocmask(SIG_SETMASK, &saved, NULL);
		pseudo_debug(4, "completed: execle\n");
		errno = save_errno;
		free(argv);
		return rc;
	} else {
		pseudo_droplock();
		sigprocmask(SIG_SETMASK, &saved, NULL);
		pseudo_debug(4, "completed: execle\n");
		/* rc was initialized to the "failure" value */
		pseudo_enosys("execle");
		free(argv);
		return rc;
	}
}

int
fork(void) {
	sigset_t saved;
	
	int rc = -1;

	pseudo_debug(4, "called: fork\n");
	pseudo_sigblock(&saved);
	if (pseudo_getlock()) {
		errno = EBUSY;
		sigprocmask(SIG_SETMASK, &saved, NULL);
		return -1;
	}
	if (pseudo_populate_wrappers()) {
		int save_errno;
			
		rc = wrap_fork();

		save_errno = errno;
		
		pseudo_droplock();
		sigprocmask(SIG_SETMASK, &saved, NULL);
		pseudo_debug(4, "completed: fork\n");
		errno = save_errno;
		return rc;
	} else {
		pseudo_droplock();
		sigprocmask(SIG_SETMASK, &saved, NULL);
		pseudo_debug(4, "completed: fork\n");
		/* rc was initialized to the "failure" value */
		pseudo_enosys("fork");
		
		return rc;
	}
}

int
vfork(void) {
	/* we don't provide support for the distinct semantics
	 * of vfork()
	 */
	return fork();
}

int
wrap_clone(int (*fn)(void *), void *child_stack, int flags, void *arg, ...) {
	/* unused */
	return 0;
}

int
clone(int (*fn)(void *), void *child_stack, int flags, void *arg, ...) {
	sigset_t saved;
	va_list ap;
	pid_t *pid;
	struct user_desc *tls;
	pid_t *ctid;

	int rc = -1;

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
	if (pseudo_populate_wrappers()) {
		int save_errno;
		int save_disabled = pseudo_disabled;
		/* because clone() doesn't actually continue in this function, we
		 * can't check the return and fix up environment variables in the
		 * child.  Instead, we have to temporarily do any fixup, then possibly
		 * undo it later.  UGH!
		 */
		pseudo_debug(1, "client resetting for clone(2) call\n");
		if (real_clone) {
			pseudo_setupenv();
			pseudo_client_reset();
			/* call the real syscall */
			rc = (*real_clone)(fn, child_stack, flags, arg, pid, tls, ctid);

			/* if we got here, we're the parent process.  And if we changed
			 * pseudo_disabled because of the environment, now we want to
			 * bring it back.  We can't use the normal path for this in
			 * pseudo_client_reset() because that would trust the environment
			 * variable, which was intended only to modify the behavior of
			 * the child process.
			 */
			if (save_disabled != pseudo_disabled) {
				if (pseudo_disabled) {
					pseudo_disabled = 0;
					pseudo_magic();
				} else {
					pseudo_disabled = 1;
					pseudo_antimagic();
				}
			}
		} else {
			/* rc was initialized to the "failure" value */
			pseudo_enosys("clone");
		}
		
		save_errno = errno;
		pseudo_droplock();
		sigprocmask(SIG_SETMASK, &saved, NULL);
		pseudo_debug(4, "completed: clone\n");
		errno = save_errno;
		return rc;
	} else {
		pseudo_droplock();
		sigprocmask(SIG_SETMASK, &saved, NULL);
		pseudo_debug(4, "completed: clone\n");
		/* rc was initialized to the "failure" value */
		pseudo_enosys("clone");
		
		return rc;
	}
}

static int (*real_fork)(void) = NULL;
static int (*real_execlp)(const char *file, const char *arg, ...) = NULL;
static int (*real_execl)(const char *file, const char *arg, ...) = NULL;
static int (*real_execle)(const char *file, const char *arg, ...) = NULL;
static int (*real_clone)(int (*)(void *), void *, int, void *, ...) = NULL;

static int
wrap_fork(void) {
	int rc = -1;
	
#include "guts/fork.c"

	return rc;
}
