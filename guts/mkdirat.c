/* 
 * static int
 * wrap_mkdirat(int dirfd, const char *path, mode_t mode) {
 *	int rc = -1;
 */
#ifdef PSEUDO_NO_REAL_AT_FUNCTIONS
	if (dirfd != AT_FDCWD) {
		errno = ENOSYS;
		return -1;
	}
	rc = real_mkdir(path, PSEUDO_FS_MODE(mode));
#else
	rc = real_mkdirat(dirfd, path, PSEUDO_FS_MODE(mode));
#endif
	if (rc != -1) {
		struct stat64 buf;
		int stat_rc;

#ifdef PSEUDO_NO_REAL_AT_FUNCTIONS
		stat_rc = real___lxstat64(_STAT_VER, path, &buf);
#else
		stat_rc = real___fxstatat64(_STAT_VER, dirfd, path, &buf, AT_SYMLINK_NOFOLLOW);
#endif
		if (stat_rc != -1) {
			pseudo_client_op(OP_MKDIR, -1, dirfd, path, &buf);
		} else {
			pseudo_debug(1, "mkdir of %s succeeded, but stat failed: %s\n",
				path, strerror(errno));
		}
	}

/*	return rc;
 * }
 */
