/* 
 * Copyright (c) 2008-2010, 2012 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * static int
 * wrap_symlinkat(const char *oldname, int dirfd, const char *newpath) {
 *	int rc = -1;
 */
 	PSEUDO_STATBUF buf;
	char *roldname = 0;

	if (oldname[0] == '/' && pseudo_chroot_len && !pseudo_nosymlinkexp) {
		size_t len = pseudo_chroot_len + strlen(oldname) + 1;
		roldname = malloc(len);
		snprintf(roldname, len, "%s%s", pseudo_chroot, oldname);
	}

#ifdef PSEUDO_NO_REAL_AT_FUNCTIONS
	if (dirfd != AT_FDCWD) {
		errno = ENOSYS;
		return -1;
	}
	rc = real_symlink(roldname ? roldname : oldname, newpath);
#else
	rc = real_symlinkat(roldname ? roldname : oldname, dirfd, newpath);
#endif

	if (rc == -1) {
		free(roldname);
		return rc;
	}
#ifdef PSEUDO_NO_REAL_AT_FUNCTIONS
	rc = base_lstat(newpath, &buf);
#else
	rc = base_fstatat(dirfd, newpath, &buf, AT_SYMLINK_NOFOLLOW);
#endif
	if (rc == -1) {
		int save_errno = errno;
		pseudo_diag("symlinkat: couldn't stat '%s' even though symlink creation succeeded (%s).\n",
			newpath, strerror(errno));
		errno = save_errno;
		free(roldname);
		return rc;
	}
	/* just record the entry */
	pseudo_client_op(OP_SYMLINK, 0, -1, dirfd, newpath, &buf);

	free(roldname);

/*	return rc;
 * }
 */
