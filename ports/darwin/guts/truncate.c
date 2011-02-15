/*
 * Copyright (c) 2011 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * int truncate(const char *path, off_t length)
 *	int rc = -1;
 */

	rc = real_truncate(path, length);

/*	return rc;
 * }
 */
