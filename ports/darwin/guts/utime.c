/*
 * Copyright (c) 2011 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * int utime(const char *path, const struct utimbuf *buf)
 *	int rc = -1;
 */

	rc = real_utime(path, buf);

/*	return rc;
 * }
 */
