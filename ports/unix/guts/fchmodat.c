/* 
 * Copyright (c) 2008-2010, 2012, 2013 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * static int
 * wrap_fchmodat(int dirfd, const char *path, mode_t mode, int flags) {
 *	int rc = -1;
 */
	PSEUDO_STATBUF buf;
	int save_errno = errno;

#ifdef PSEUDO_NO_REAL_AT_FUNCTIONS
	if (dirfd != AT_FDCWD) {
		errno = ENOSYS;
		return -1;
	}
	if (flags & AT_SYMLINK_NOFOLLOW) {
		rc = base_lstat(path, &buf);
	} else {
		rc = base_stat(path, &buf);
	}
#else
	rc = base_fstatat(dirfd, path, &buf, flags);
#endif
	if (rc == -1) {
		return rc;
	}
	if (S_ISLNK(buf.st_mode)) {
		/* we don't really support chmod of a symlink */
		errno = ENOSYS;
		return -1;
	}
	save_errno = errno;

#if 0
 	pseudo_msg_t *msg;
	/* purely for debugging purposes:  check whether file
	 * is already in database. We don't need the resulting
         * information for anything. This is currently ifdefed
         * out because it's only useful when trying to track where
         * files are coming from.
	 */
	msg = pseudo_client_op(OP_STAT, 0, -1, -1, path, &buf);
	if (!msg || msg->result != RESULT_SUCCEED) {
		pseudo_debug(2, "chmodat to 0%o on %d/%s, ino %llu, new file.\n",
			mode, dirfd, path, (unsigned long long) buf.st_ino);

	}
#endif

	/* user bits added so "root" can always access files. */
#ifdef PSEUDO_NO_REAL_AT_FUNCTIONS
	/* note:  if path was a symlink, and AT_NOFOLLOW_SYMLINKS was
	 * specified, we already bailed previously. */
	real_chmod(path, PSEUDO_FS_MODE(mode, S_ISDIR(buf.st_mode)));
#else
	real_fchmodat(dirfd, path, PSEUDO_FS_MODE(mode, S_ISDIR(buf.st_mode)), flags);
#endif
	/* we ignore a failure from underlying fchmod, because pseudo
	 * may believe you are permitted to change modes that the filesystem
	 * doesn't. Note that we also don't need to know whether the
         * file might be a (pseudo) block device or some such; pseudo
         * will only modify permission bits based on an OP_CHMOD, and does
         * not care about device/file type mismatches, only directory/file
         * or symlink/file.
	 */

	buf.st_mode = (buf.st_mode & ~07777) | (mode & 07777);
	pseudo_client_op(OP_CHMOD, 0, -1, dirfd, path, &buf);
        /* don't change errno from what it was originally */
        errno = save_errno;
        rc = 0;

/*	return rc;
 * }
 */
