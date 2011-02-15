/*
 * Copyright (c) 2011 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * int chroot(const char *path)
 *	int rc = -1;
 */

	rc = real_chroot(path);

/*	return rc;
 * }
 */
