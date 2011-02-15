/*
 * Copyright (c) 2011 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * int mkfifoat(int dirfd, const char *path, mode_t mode)
 *	int rc = -1;
 */

	rc = real_mkfifoat(dirfd, path, mode);

/*	return rc;
 * }
 */
