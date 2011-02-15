/*
 * Copyright (c) 2011 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * int eaccess(const char *path, int mode)
 *	int rc = -1;
 */

	rc = real_eaccess(path, mode);

/*	return rc;
 * }
 */
