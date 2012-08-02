/* 
 * Copyright (c) 2010, 2012 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * static int
 * wrap_access(const char *path, int mode) {
 *	int rc = -1;
 */
	PSEUDO_STATBUF buf;

	/* note:  no attempt to handle the case where user isn't
	 * root.
	 */
	rc = base_stat(path, &buf);
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
