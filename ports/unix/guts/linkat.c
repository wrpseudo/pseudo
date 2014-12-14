/*
 * Copyright (c) 2012, 2013 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * int linkat(int olddirfd, const char *oldname, int newdirfd, const char *newname, int flags)
 *	int rc = -1;
 */

        int rc2, rflags, save_errno;
 	pseudo_msg_t *msg;
        char *oldpath = NULL, *newpath = NULL;
 	PSEUDO_STATBUF buf;

        /* This is gratuitously complicated. On Linux 2.6.18 and later,
         * flags may contain AT_SYMLINK_FOLLOW, which implies following
         * symlinks; otherwise, linkat() will *not* follow symlinks. FreeBSD
         * appears to use the same semantics.
         *
         * So on Darwin, always pass AT_SYMLINK_FOLLOW, because the
         * alternative doesn't work. And never pass AT_SYMLINK_NOFOLLOW
         * because that's not a valid flag to linkat().
         *
         * So we need a flag for path resolution which is AT_SYMLINK_NOFOLLOW
         * unless AT_SYMLINK_FOLLOW was specified, in which case it's 0.
         */
        
        rflags = (flags & AT_SYMLINK_FOLLOW) ? 0 : AT_SYMLINK_NOFOLLOW;

#ifdef PSEUDO_NO_REAL_AT_FUNCTIONS
	if (olddirfd != AT_FDCWD || newdirfd != AT_FDCWD) {
		errno = ENOSYS;
		return -1;
	}
#endif
        oldpath = pseudo_root_path(__func__, __LINE__, olddirfd, oldname, rflags);
        newpath = pseudo_root_path(__func__, __LINE__, newdirfd, newname, AT_SYMLINK_NOFOLLOW);
        rc = real_link(oldpath, newpath);
        save_errno = errno;
        if (rc == -1) {
                free(oldpath);
                free(newpath);
                errno = save_errno;
                return rc;
        }

        /* if we got this far, the link succeeded, and oldpath and newpath
         * are the newly-allocated canonical paths. If OS, filesystem, or
         * the flags value prevent hard linking to symlinks, the resolved
         * path should be the target's path anyway, so lstat is safe here.
         */
        /* find the target: */
        rc2 = base_lstat(oldpath, &buf);
        if (rc2 == -1) {
                pseudo_diag("Fatal: Tried to stat '%s' after linking it, but failed: %s.\n",
                        oldpath, strerror(errno));
                free(oldpath);
                free(newpath);
                errno = ENOENT;
                return rc;
        }
        msg = pseudo_client_op(OP_STAT, 0, -1, -1, oldpath, &buf);
        if (msg) {
                pseudo_stat_msg(&buf, msg);
        }
        /* Long story short: I am pretty sure we still want OP_LINK even
         * if the thing linked is a symlink.
         */
        pseudo_client_op(OP_LINK, 0, -1, -1, newpath, &buf);

        free(oldpath);
        free(newpath);
        errno = save_errno;

/*	return rc;
 * }
 */
