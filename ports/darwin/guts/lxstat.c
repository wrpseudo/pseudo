/*
 * Copyright (c) 2011 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * int lxstat(const char *path, struct stat *buf)
 *	int rc = -1;
 */

	rc = real_lxstat(path, buf);

/*	return rc;
 * }
 */
