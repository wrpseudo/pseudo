/* 
 * Copyright (c) 2008-2010, 2012 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * static int
 * wrap_fchmodat(int dirfd, const char *path, mode_t mode, int flags) {
 *	int rc = -1;
 */
 	pseudo_msg_t *msg;
	PSEUDO_STATBUF buf;
	int save_errno;

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

	/* purely for debugging purposes:  check whether file
	 * is already in database.
	 */
	msg = pseudo_client_op(OP_STAT, 0, -1, -1, path, &buf);
	if (!msg || msg->result != RESULT_SUCCEED) {
		pseudo_debug(2, "chmodat to 0%o on %d/%s, ino %llu, new file.\n",
			mode, dirfd, path, (unsigned long long) buf.st_ino);

	}

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
	 * doesn't.
	 */

	buf.st_mode = (buf.st_mode & ~07777) | (mode & 07777);
	msg = pseudo_client_op(OP_CHMOD, 0, -1, dirfd, path, &buf);
	if (msg && msg->result != RESULT_SUCCEED) {
		errno = EPERM;
		rc = -1;
	} else {
		/* if server is down, just pretend we worked */
		rc = 0;
	}

/*	return rc;
 * }
 */
