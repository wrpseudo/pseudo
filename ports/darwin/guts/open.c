/*
 * Copyright (c) 2011-2013 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * int open(const char *path, int flags, ... { int mode })
 *	int rc = -1;
 */

	struct stat buf = { };
	int existed = 1;
	int save_errno;
#ifdef PSEUDO_FORCE_ASYNCH
        flags &= ~O_SYNC;
#endif

	/* if a creation has been requested, check whether file exists */
	if (flags & O_CREAT) {
		save_errno = errno;
		rc = real_stat(path, &buf);
		existed = (rc != -1);
		if (!existed)
			pseudo_debug(2, "open_creat: %s -> 0%o\n", path, mode);
		errno = save_errno;
	}

	/* because we are not actually root, secretly mask in 0600 to the
	 * underlying mode.  The ", 0" is because the only time mode matters
	 * is if a file is going to be created, in which case it's
	 * not a directory.
	 */
	rc = real_open(path, flags, PSEUDO_FS_MODE(mode, 0));
	save_errno = errno;

	if (rc != -1) {
		int stat_rc;
		stat_rc = real_stat(path, &buf);

		if (stat_rc != -1) {
			buf.st_mode = PSEUDO_DB_MODE(buf.st_mode, mode);
			if (!existed) {
				pseudo_client_op(OP_CREAT, 0, -1, -1, path, &buf);
			}
			pseudo_client_op(OP_OPEN, PSEUDO_ACCESS(flags), rc, -1, path, &buf);
		} else {
			pseudo_debug(1, "open (fd %d, path %s, flags %d) succeeded, but stat failed (%s).\n",
				rc, path, flags, strerror(errno));
			pseudo_client_op(OP_OPEN, PSEUDO_ACCESS(flags), rc, -1, path, 0);
		}
		errno = save_errno;
	}

/*	return rc;
 * }
 */
