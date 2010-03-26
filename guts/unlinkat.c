/* 
 * static int
 * wrap_unlinkat(int dirfd, const char *path, int rflags) {
 *	int rc = -1;
 */
#ifdef PSEUDO_NO_REAL_AT_FUNCTIONS
	if (dirfd != AT_FDCWD) {
		errno = ENOSYS;
		return -1;
	}
	if (rflags) {
		/* the only supported flag is AT_REMOVEDIR.  We'd never call
		 * with that flag unless the real AT functions exist, so 
		 * something must have gone horribly wrong....
		 */
		pseudo_diag("wrap_unlinkat called with flags (0x%x), path '%s'\n",
			rflags, path ? path : "<nil>");
		errno = ENOSYS;
		return -1;
	}
#endif
 	struct stat64 buf;

#ifdef PSEUDO_NO_REAL_AT_FUNCTIONS
	rc = real___lxstat64(_STAT_VER, path, &buf);
#else
	rc = real___fxstatat64(_STAT_VER, dirfd, path, &buf, AT_SYMLINK_NOFOLLOW);
#endif
	if (rc == -1) {
		return rc;
	}
#ifdef PSEUDO_NO_REAL_AT_FUNCTIONS
	rc = real_unlink(path);
#else
	rc = real_unlinkat(dirfd, path, rflags);
#endif
	if (rc != -1) {
		pseudo_client_op(OP_UNLINK, 0, -1, dirfd, path, &buf);
	}

/*	return rc;
 * }
 */
