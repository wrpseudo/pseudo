/* 
 * Copyright (c) 2008-2010 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * static int
 * wrap_remove(const char *path) {
 *	int rc = -1;
 */
	struct stat buf;
	if (real_lstat(path, &buf) == -1) {
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
