/* 
 * static int
 * wrap_access(const char *path, int mode) {
 *	int rc = -1;
 */
	struct stat64 buf;

	/* note:  no attempt to handle the case where user isn't
	 * root.
	 */
	rc = wrap___fxstatat64(_STAT_VER, AT_FDCWD, path, &buf, 0);
	if (rc == -1)
		return rc;

	if (mode & X_OK) {
		if (buf.st_mode & 0111) {
			return 0;
		} else {
			errno = EPERM;
			return -1;
		}
	} else {
		return 0;
	}

/*	return rc;
 * }
 */
