/*
 * Copyright (c) 2011 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * int symlink(const char *oldname, const char *newpath)
 *	int rc = -1;
 */

	rc = real_symlink(oldname, newpath);

/*	return rc;
 * }
 */
