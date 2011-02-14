/*
 * Copyright (c) 2011 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * ssize_t readlink(const char *path, char *buf, size_t bufsiz)
 *	ssize_t rc = -1;
 */

	rc = real_readlink(path, buf, bufsiz);

/*	return rc;
 * }
 */
