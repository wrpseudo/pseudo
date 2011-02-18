/*
 * Copyright (c) 2011 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * int open(const char *path, int flags, ... { int mode })
 *	int rc = -1;
 */

	struct stat buf;
	int existed = 1;
	int save_errno;

	/* if a creation has been requested, check whether file exists */
	if (flags & O_CREAT) {
		save_errno = errno;
		rc = real_stat(path, &buf);
		existed = (rc != -1);
		if (!existed)
			pseudo_debug(2, "open_creat: %s -> 0%o\n", path, mode);
		errno = save_errno;
	}

	/* because we are not actually root, secretly mask in 0700 to the
	 * underlying mode
	 */
	rc = real_open(path, flags, PSEUDO_FS_MODE(mode));
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
