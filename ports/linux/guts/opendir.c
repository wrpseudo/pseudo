/* 
 * Copyright (c) 2010 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * static DIR *
 * wrap_opendir(const char *path) {
 *	DIR * rc = NULL;
 */

	rc = real_opendir(path);

/*	return rc;
 * }
 */
