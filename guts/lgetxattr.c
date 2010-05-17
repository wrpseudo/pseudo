/* 
 * Copyright (c) 2010 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * static ssize_t
 * wrap_lgetxattr(const char *path, const char *name, void *value, size_t size) {
 *	ssize_t rc = -1;
 */

	rc = real_lgetxattr(path, name, value, size);

/*	return rc;
 * }
 */
