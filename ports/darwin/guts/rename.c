/*
 * Copyright (c) 2011 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * int rename(const char *oldpath, const char *newpath)
 *	int rc = -1;
 */

	rc = real_rename(oldpath, newpath);

/*	return rc;
 * }
 */
