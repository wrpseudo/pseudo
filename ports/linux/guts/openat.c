/* 
 * Copyright (c) 2008-2010, 2013 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * static int
 * wrap_openat(int dirfd, const char *path, int flags, ...mode_t mode) {
 *	int rc = -1;
 */
	struct stat64 buf;
	int existed = 1;
	int save_errno;

	/* mask out mode bits appropriately */
	mode = mode & ~pseudo_umask;

#ifdef PSEUDO_NO_REAL_AT_FUNCTIONS
	if (dirfd != AT_FDCWD) {
		errno = ENOSYS;
		return -1;
	}
#endif

#ifdef PSEUDO_FORCE_ASYNCH
        /* Yes, I'm aware that every Linux system I've seen has
         * DSYNC and RSYNC being the same value as SYNC.
         */

        flags &= ~(O_SYNC
#ifdef O_DIRECT
                | O_DIRECT
#endif
#ifdef O_DSYNC
                | O_DSYNC
#endif
#ifdef O_RSYNC
                | O_RSYNC
#endif
        );
#endif

	/* if a creation has been requested, check whether file exists */
	if (flags & O_CREAT) {
		save_errno = errno;
#ifdef PSEUDO_NO_REAL_AT_FUNCTIONS
		rc = real___xstat64(_STAT_VER, path, &buf);
#else
		rc = real___fxstatat64(_STAT_VER, dirfd, path, &buf, 0);
#endif
		existed = (rc != -1);
		if (!existed)
			pseudo_debug(PDBGF_FILE, "openat_creat: %s -> 0%o\n", path, mode);
		errno = save_errno;
	}

	/* because we are not actually root, secretly mask in 0600 to the
	 * underlying mode.  The ", 0" is because the only time mode matters
	 * is if a file is going to be created, in which case it's
	 * not a directory.
	 */
#ifdef PSEUDO_NO_REAL_AT_FUNCTIONS
	rc = real_open(path, flags, PSEUDO_FS_MODE(mode, 0));
#else
	rc = real_openat(dirfd, path, flags, PSEUDO_FS_MODE(mode, 0));
#endif
	save_errno = errno;

	if (rc != -1) {
		int stat_rc;
#ifdef PSEUDO_NO_REAL_AT_FUNCTIONS
		stat_rc = real___xstat64(_STAT_VER, path, &buf);
#else
		stat_rc = real___fxstatat64(_STAT_VER, dirfd, path, &buf, 0);
#endif

		if (stat_rc != -1) {
			buf.st_mode = PSEUDO_DB_MODE(buf.st_mode, mode);
			if (!existed) {
				pseudo_client_op(OP_CREAT, 0, -1, dirfd, path, &buf);
			}
			pseudo_client_op(OP_OPEN, PSEUDO_ACCESS(flags), rc, dirfd, path, &buf);
		} else {
			pseudo_debug(PDBGF_FILE, "openat (fd %d, path %d/%s, flags %d) succeeded, but stat failed (%s).\n",
				rc, dirfd, path, flags, strerror(errno));
			pseudo_client_op(OP_OPEN, PSEUDO_ACCESS(flags), rc, dirfd, path, 0);
		}
		errno = save_errno;
	}

/*	return rc;
 * }
 */
