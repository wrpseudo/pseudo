/*
 * Copyright (c) 2014 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * int removexattr(const char *path, const char *name, int options)
 *	int rc = -1;
 */

	rc = shared_removexattr(path, -1, name, options);

/*	return rc;
 * }
 */
