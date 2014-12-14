/* 
 * Copyright (c) 2008-2010 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * static int
 * wrap___xmknodat(int ver, int dirfd, const char *path, mode_t mode, dev_t *dev) {
 *	int rc = -1;
 */
 	pseudo_msg_t *msg;
	struct stat64 buf;

	/* mask out mode bits appropriately */
	mode = mode & ~pseudo_umask;

	/* we don't use underlying call, so _ver is irrelevant to us */
	(void) ver;

#ifdef PSEUDO_NO_REAL_AT_FUNCTIONS
	if (dirfd != AT_FDCWD) {
		errno = ENOSYS;
		return -1;
	}
	rc = real___xstat64(_STAT_VER, path, &buf);
#else
	rc = real___fxstatat64(_STAT_VER, dirfd, path, &buf, AT_SYMLINK_NOFOLLOW);
#endif
	if (rc != -1) {
		/* if we can stat the file, you can't mknod it */
		errno = EEXIST;
		return -1;
	}
	if (!dev) {
		errno = EINVAL;
		return -1;
	}
#ifdef PSEUDO_NO_REAL_AT_FUNCTIONS
	rc = real_open(path, O_CREAT | O_WRONLY | O_EXCL,
		PSEUDO_FS_MODE(mode, 0));
#else
	rc = real_openat(dirfd, path, O_CREAT | O_WRONLY | O_EXCL,
		PSEUDO_FS_MODE(mode, 0));
#endif
	if (rc == -1) {
		return -1;
	}
	real___fxstat64(_STAT_VER, rc, &buf);
	/* mknod does not really open the file.  We don't have
	 * to use wrap_close because we've never exposed this file
	 * descriptor to the client code.
	 */
	real_close(rc);

	/* mask in the mode type bits again */
	buf.st_mode = (PSEUDO_DB_MODE(buf.st_mode, mode) & 07777) |
			(mode & ~07777);
	buf.st_rdev = *dev;
	msg = pseudo_client_op(OP_MKNOD, 0, -1, dirfd, path, &buf);
	if (msg && msg->result != RESULT_SUCCEED) {
		errno = EPERM;
		rc = -1;
	} else {
		/* just pretend we worked */
		rc = 0;
	}
	if (rc == -1) {
		int save_errno = errno;
#ifdef PSEUDO_NO_REAL_AT_FUNCTIONS
		real_unlink(path);
#else
		real_unlinkat(dirfd, path, AT_SYMLINK_NOFOLLOW);
#endif
		errno = save_errno;
	}

/*	return rc;
 * }
 */
