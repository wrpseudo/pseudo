/*
 * Copyright (c) 2011 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * int removexattr(const char *pathname, const char *name)
 *	int rc = -1;
 */

	rc = real_removexattr(pathname, name);

/*	return rc;
 * }
 */
