/*
 * Copyright (c) 2011 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * int lremovexattr(const char *pathname, const char *name)
 *	int rc = -1;
 */

	rc = real_lremovexattr(pathname, name);

/*	return rc;
 * }
 */
