/*
 * Copyright (c) 2011 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * long pathconf(const char *path, int name)
 *	long rc = -1;
 */

	rc = real_pathconf(path, name);

/*	return rc;
 * }
 */
