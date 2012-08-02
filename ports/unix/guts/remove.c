/* 
 * Copyright (c) 2008-2010, 2012 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * static int
 * wrap_remove(const char *path) {
 *	int rc = -1;
 */
	PSEUDO_STATBUF buf;
	if (base_lstat(path, &buf) == -1) {
		errno = ENOENT;
		return -1;
	}
	if (S_ISDIR(buf.st_mode)) {
		rc = wrap_rmdir(path);
	} else {
		rc = wrap_unlink(path);
	}

/*	return rc;
 * }
 */
