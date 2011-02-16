/*
 * Copyright (c) 2011 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * int euidaccess(const char *path, int mode)
 *	int rc = -1;
 */

	rc = real_euidaccess(path, mode);

/*	return rc;
 * }
 */
