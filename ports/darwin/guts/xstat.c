/*
 * Copyright (c) 2011 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * int xstat(const char *path, struct stat *buf)
 *	int rc = -1;
 */

	rc = real_xstat(path, buf);

/*	return rc;
 * }
 */
