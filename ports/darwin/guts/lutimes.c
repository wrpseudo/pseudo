/*
 * Copyright (c) 2011 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * int lutimes(const char *path, const struct timeval *tv)
 *	int rc = -1;
 */

	rc = real_lutimes(path, tv);

/*	return rc;
 * }
 */
