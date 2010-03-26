/* 
 * static int
 * wrap___fxstatat64(int ver, int dirfd, const char *path, struct stat64 *buf, int flags) {
 *	int rc = -1;
 */
	pseudo_msg_t *msg;
	int save_errno;
	mode_t save_mode = 0;

#ifdef PSEUDO_NO_REAL_AT_FUNCTIONS
	if (dirfd != AT_FDCWD) {
		errno = ENOSYS;
		return -1;
	}
#endif
	/* If the file is actually a symlink, we grab the db values
	 * for the underlying target, then mask in the size and mode
	 * from the link.  Otherwise, we just use the db values (if
	 * any).
	 */ 
	if (flags & AT_SYMLINK_NOFOLLOW) {
#ifdef PSEUDO_NO_REAL_AT_FUNCTIONS
		rc = real___lxstat64(ver, path, buf);
#else
		rc = real___fxstatat64(ver, dirfd, path, buf, flags);
#endif
		if (rc == -1) {
			return rc;
		}
		/* it's a symlink, stash its mode */
		if (S_ISLNK(buf->st_mode)) {
			save_mode = buf->st_mode;
		}
	} else {
#ifdef PSEUDO_NO_REAL_AT_FUNCTIONS
		rc = real___xstat64(ver, path, buf);
#else
		rc = real___fxstatat64(ver, dirfd, path, buf, flags);
#endif
		if (rc == -1) {
			return rc;
		}
	}
	/* if we got here:  we have valid stat data, for either the symlink
	 * (if it is a symlink, and we have NOFOLLOW on) or the target.
	 */
	save_errno = errno;

	if (ver != _STAT_VER) {
		pseudo_debug(1, "version mismatch: got stat version %d, only supporting %d\n", ver, _STAT_VER);
		errno = save_errno;
		return rc;
	}

	/* query database
	 * note that symlink canonicalizing is now automatic, so we
	 * don't need to check for a symlink on this end
	 */
	msg = pseudo_client_op(OP_STAT, 0, -1, dirfd, path, buf);
	if (msg) {
		pseudo_stat_msg(buf, msg);
		if (save_mode) {
			buf->st_mode = save_mode;
		}
	}

	errno = save_errno;

/*	return rc;
 * }
 */
