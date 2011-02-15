/*
 * Copyright (c) 2011 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * int utimes(const char *path, const struct timeval *times)
 *	int rc = -1;
 */

	rc = real_utimes(path, times);

/*	return rc;
 * }
 */
