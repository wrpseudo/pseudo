/*
 * Copyright (c) 2011 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * int ftw(const char *path, int (*fn)(const char *, const struct stat *, int), int nopenfd)
 *	int rc = -1;
 */

	rc = real_ftw(path, fn, nopenfd);

/*	return rc;
 * }
 */
