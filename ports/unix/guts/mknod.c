/*
 * Copyright (c) 2011,2014 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * int mknod(const char *path, mode_t mode, dev_t dev)
 *	int rc = -1;
 */

	rc = wrap_mknodat(AT_FDCWD, path, mode, dev);

/*	return rc;
 * }
 */
