int
wrap_clone(int (*fn)(void *), void *child_stack, int flags, void *arg) {
	/* unused */
	return 0;
}

int
clone(int (*fn)(void *), void *child_stack, int flags, void *arg) {
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
