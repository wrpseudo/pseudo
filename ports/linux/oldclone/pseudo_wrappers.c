static int
wrap_clone(int (*fn)(void *), void *child_stack, int flags, void *arg) {
	/* unused */
	return 0;
}

struct clone_args {
	int (*fn)(void *);
	int flags;
	void *arg;
};

int wrap_clone_child(void *args) {
	struct clone_args *clargs = args;

	int (*fn)(void *) = clargs->fn;
	int flags = clargs->flags;
	void *arg = clargs->arg;

	/* We always free in the client */
	free(clargs);

	if (!(flags & CLONE_VM)) {
		pseudo_setupenv();
		if (!pseudo_has_unload(NULL)) {
			pseudo_reinit_libpseudo();
		} else {
			pseudo_dropenv();
		}
	}

	return fn(arg);
}

int
clone(int (*fn)(void *), void *child_stack, int flags, void *arg) {
	sigset_t saved;

	int rc = -1;

	if (!pseudo_check_wrappers() || !real_clone) {
		/* rc was initialized to the "failure" value */
		pseudo_enosys("clone");
		return rc;
	}

	pseudo_debug(PDBGF_WRAPPER, "called: clone\n");
	pseudo_sigblock(&saved);
	if (pseudo_getlock()) {
		errno = EBUSY;
		sigprocmask(SIG_SETMASK, &saved, NULL);
		return -1;
	}

	int save_errno;
	int save_disabled = pseudo_disabled;

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
	pseudo_debug(PDBGF_WRAPPER, "completed: clone\n");
	errno = save_errno;
	return rc;
}
