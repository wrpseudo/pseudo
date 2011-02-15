/*
 * Copyright (c) 2011 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * int creat(const char *path, mode_t mode)
 *	int rc = -1;
 */

	rc = real_creat(path, mode);

/*	return rc;
 * }
 */
