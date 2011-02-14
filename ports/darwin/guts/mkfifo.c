/*
 * Copyright (c) 2011 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * int mkfifo(const char *path, mode_t mode)
 *	int rc = -1;
 */

	rc = real_mkfifo(path, mode);

/*	return rc;
 * }
 */
