/* 
 * Copyright (c) 2010 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * static int
 * wrap_utimes(const char *path, const struct timeval *times) {
 *	int rc = -1;
 */
	rc = real_utimes(path, times);

/*	return rc;
 * }
 */
