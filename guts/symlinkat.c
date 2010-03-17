/* 
 * static int
 * wrap_symlinkat(const char *oldpath, int dirfd, const char *newpath) {
 *	int rc = -1;
 */
 	struct stat64 buf;

#ifdef PSEUDO_NO_REAL_AT_FUNCTIONS
	if (dirfd != AT_FDCWD) {
		errno = ENOSYS;
		return -1;
	}
	rc = real_symlink(oldpath, newpath);
#else
	rc = real_symlinkat(oldpath, dirfd, newpath);
#endif

	if (rc == -1) {
		return rc;
	}
#ifdef PSEUDO_NO_REAL_AT_FUNCTIONS
	rc = real___lxstat64(_STAT_VER, newpath, &buf);
#else
	rc = real___fxstatat64(_STAT_VER, dirfd, newpath, &buf, AT_SYMLINK_NOFOLLOW);
#endif
	if (rc == -1) {
		int save_errno = errno;
		pseudo_diag("symlinkat: couldn't stat '%s' even though symlink creation succeeded (%s).\n",
			newpath, strerror(errno));
		errno = save_errno;
		return rc;
	}
	/* just record the entry */
	pseudo_client_op(OP_SYMLINK, AT_SYMLINK_NOFOLLOW, -1, dirfd, newpath, &buf);

/*	return rc;
 * }
 */
