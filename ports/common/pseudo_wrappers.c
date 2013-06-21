/* these aren't used, but the wrapper table isn't happy unless they
 * exist
 */
static int
wrap_execl(const char *file, const char *arg, va_list ap) {
	(void) file;
	(void) arg;
	(void) ap;
	return 0; 
}

static int
wrap_execle(const char *file, const char *arg, va_list ap) {
	(void) file;
	(void) arg;
	(void) ap;
	return 0;
}

static int
wrap_execlp(const char *file, const char *arg, va_list ap) {
	(void) file;
	(void) arg;
	(void) ap;
	return 0;
}

static char **
execl_to_v(va_list ap, const char *argv0, char *const **envp) {
	size_t i = 0;
	size_t alloc_size = 256;

	char **argv = malloc((sizeof *argv) * alloc_size);

	if (!argv) {
		pseudo_debug(PDBGF_CLIENT, "execl failed: couldn't allocate memory for %lu arguments\n",
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
				pseudo_debug(PDBGF_CLIENT, "execl failed: couldn't allocate memory for %lu arguments\n",
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

	pseudo_debug(PDBGF_WRAPPER, "called: execl\n");
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
	pseudo_debug(PDBGF_WRAPPER, "completed: execl\n");
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

	pseudo_debug(PDBGF_WRAPPER, "called: execlp\n");
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
	pseudo_debug(PDBGF_WRAPPER, "completed: execlp\n");
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

	pseudo_debug(PDBGF_WRAPPER, "called: execle\n");
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
	pseudo_debug(PDBGF_WRAPPER, "completed: execle\n");
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

	pseudo_debug(PDBGF_WRAPPER, "called: execv\n");
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
	pseudo_debug(PDBGF_WRAPPER, "completed: execv\n");
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

	pseudo_debug(PDBGF_WRAPPER, "called: execve\n");
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
	pseudo_debug(PDBGF_WRAPPER, "completed: execve\n");
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

	pseudo_debug(PDBGF_WRAPPER, "called: execvp\n");
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
	pseudo_debug(PDBGF_WRAPPER, "completed: execvp\n");
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

	pseudo_debug(PDBGF_WRAPPER, "called: fork\n");
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
	pseudo_debug(PDBGF_WRAPPER, "completed: fork\n");
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

