FILE *
popen(const char *command, const char *mode) {
	sigset_t saved;
	
	FILE *rc = NULL;

	if (!pseudo_check_wrappers() || !real_popen) {
		/* rc was initialized to the "failure" value */
		pseudo_enosys("popen");
		return rc;
	}

	pseudo_debug(PDBGF_WRAPPER, "called: popen\n");
	pseudo_sigblock(&saved);
	if (pseudo_getlock()) {
		errno = EBUSY;
		sigprocmask(SIG_SETMASK, &saved, NULL);
		return NULL;
	}

	int save_errno;
	/* exec*() use this to restore the sig mask */
	pseudo_saved_sigmask = saved;
	rc = wrap_popen(command, mode);
	
	save_errno = errno;
	pseudo_droplock();
	sigprocmask(SIG_SETMASK, &saved, NULL);
#if 0
/* This can cause hangs on some recentish systems which use locale
 * stuff for strerror...
 */
	pseudo_debug(PDBGF_WRAPPER, "completed: popen (maybe: %s)\n", strerror(save_errno));
#endif
	pseudo_debug(PDBGF_WRAPPER, "completed: popen (errno: %d)\n", save_errno);
	errno = save_errno;
	return rc;
}

static FILE *
wrap_popen(const char *command, const char *mode) {
	FILE *rc = NULL;
	
	

#include "guts/popen.c"

	return rc;
}

