/*
 * Copyright (c) 2011 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * int access(const char *path, int mode)
 *	int rc = -1;
 */

	rc = real_access(path, mode);

/*	return rc;
 * }
 */
