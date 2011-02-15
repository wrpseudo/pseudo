/*
 * Copyright (c) 2011 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * int open(const char *path, int flags, ... { mode_t mode })
 *	int rc = -1;
 */

	rc = real_open(path, flags, mode);

/*	return rc;
 * }
 */
