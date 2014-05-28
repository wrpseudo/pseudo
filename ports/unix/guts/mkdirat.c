/* 
 * Copyright (c) 2008-2010, 2012 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * static int
 * wrap_mkdirat(int dirfd, const char *path, mode_t mode) {
 *	int rc = -1;
 */
	/* mask out mode bits appropriately */
	mode = mode & ~pseudo_umask;
#ifdef PSEUDO_NO_REAL_AT_FUNCTIONS
	if (dirfd != AT_FDCWD) {
		errno = ENOSYS;
		return -1;
	}

	rc = real_mkdir(path, PSEUDO_FS_MODE(mode, 1));
#else
	rc = real_mkdirat(dirfd, path, PSEUDO_FS_MODE(mode, 1));
#endif
	if (rc != -1) {
		PSEUDO_STATBUF buf;
		int stat_rc;

#ifdef PSEUDO_NO_REAL_AT_FUNCTIONS
		stat_rc = base_lstat(path, &buf);
#else
		stat_rc = base_fstatat(dirfd, path, &buf, AT_SYMLINK_NOFOLLOW);
#endif
		if (stat_rc != -1) {
			buf.st_mode = PSEUDO_DB_MODE(buf.st_mode, mode);
			pseudo_client_op(OP_MKDIR, 0, -1, dirfd, path, &buf);
		} else {
			pseudo_debug(PDBGF_OP, "mkdir of %s succeeded, but stat failed: %s\n",
				path, strerror(errno));
		}
	}

/*	return rc;
 * }
 */
