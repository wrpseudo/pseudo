/*
 * Copyright (c) 2011 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * int mknodat(int dirfd, const char *path, mode_t mode, dev_t dev)
 *	int rc = -1;
 */

 	pseudo_msg_t *msg;
	struct stat buf;

#ifdef PSEUDO_NO_REAL_AT_FUNCTIONS
	if (dirfd != AT_FDCWD) {
		errno = ENOSYS;
		return -1;
	}
	rc = real_stat(path, &buf);
#else
	rc = real___fxstatat(_STAT_VER, dirfd, path, &buf, AT_SYMLINK_NOFOLLOW);
#endif
	if (rc != -1) {
		/* if we can stat the file, you can't mknod it */
		errno = EEXIST;
		return -1;
	}
#ifdef PSEUDO_NO_REAL_AT_FUNCTIONS
	rc = real_open(path, O_CREAT | O_WRONLY | O_EXCL, PSEUDO_FS_MODE(mode));
#else
	rc = real_openat(dirfd, path, O_CREAT | O_WRONLY | O_EXCL,
		PSEUDO_FS_MODE(mode));
#endif
	if (rc == -1) {
		return -1;
	}
	real_fstat(rc, &buf);
	/* mknod does not really open the file.  We don't have
	 * to use wrap_close because we've never exposed this file
	 * descriptor to the client code.
	 */
	real_close(rc);

	/* mask in the mode type bits again */
	buf.st_mode = (PSEUDO_DB_MODE(buf.st_mode, mode) & 07777) |
			(mode & ~07777);
	buf.st_rdev = dev;
	msg = pseudo_client_op_plain(OP_MKNOD, 0, -1, dirfd, path, &buf);
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
