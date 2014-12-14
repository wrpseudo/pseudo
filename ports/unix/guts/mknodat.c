/*
 * Copyright (c) 2011 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * int mknodat(int dirfd, const char *path, mode_t mode, dev_t dev)
 *	int rc = -1;
 */

 	pseudo_msg_t *msg;
	PSEUDO_STATBUF buf;
        int save_errno = errno;

	/* mask out mode bits appropriately */
	mode = mode & ~pseudo_umask;

#ifdef PSEUDO_NO_REAL_AT_FUNCTIONS
	if (dirfd != AT_FDCWD) {
		errno = ENOSYS;
		return -1;
	}
	rc = base_stat(path, &buf);
#else
	rc = base_fstatat(dirfd, path, &buf, AT_SYMLINK_NOFOLLOW);
#endif
	if (rc != -1) {
		/* if we can stat the file, you can't mknod it */
		errno = EEXIST;
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
	base_fstat(rc, &buf);
	/* mknod does not really open the file.  We don't have
	 * to use wrap_close because we've never exposed this file
	 * descriptor to the client code.
	 */
	real_close(rc);

	/* mask in the mode type bits again */
	buf.st_mode = (PSEUDO_DB_MODE(buf.st_mode, mode) & 07777) |
			(mode & ~07777);
	buf.st_rdev = dev;
	msg = pseudo_client_op(OP_MKNOD, 0, -1, dirfd, path, &buf);
	if (msg && msg->result != RESULT_SUCCEED) {
		errno = EPERM;
		rc = -1;
	} else {
		/* just pretend we worked */
                errno = save_errno;
		rc = 0;
	}
	if (rc == -1) {
                save_errno = errno;
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
