/* 
 * static int
 * wrap_openat(int dirfd, const char *path, int flags, ...mode_t mode) {
 *	int rc = -1;
 */
	struct stat64 buf;
	int existed = 1;
	int save_errno;

#ifdef PSEUDO_NO_REAL_AT_FUNCTIONS
	if (dirfd != AT_FDCWD) {
		errno = ENOSYS;
		return -1;
	}
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
			pseudo_debug(2, "openat_creat: %s -> 0%o\n", path, mode);
		errno = save_errno;
	}

	/* because we are not actually root, secretly mask in 0700 to the
	 * underlying mode
	 */
#ifdef PSEUDO_NO_REAL_AT_FUNCTIONS
	rc = real_open(path, flags, PSEUDO_FS_MODE(mode));
#else
	rc = real_openat(dirfd, path, flags, PSEUDO_FS_MODE(mode));
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
			pseudo_client_op(OP_OPEN, 0, rc, dirfd, path, &buf);
		} else {
			pseudo_debug(1, "openat (fd %d, path %d/%s, flags %d) succeeded, but stat failed (%s).\n",
				rc, dirfd, path, flags, strerror(errno));
			pseudo_client_op(OP_OPEN, 0, rc, dirfd, path, 0);
		}
		errno = save_errno;
	}

/*	return rc;
 * }
 */
