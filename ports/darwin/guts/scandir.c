/* 
 * Copyright (c) 2010 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * static int
 * wrap_scandir(const char *path, struct dirent ***namelist, int (*filter)(struct dirent *), int (*compar)(const void *, const void *)) {
 *	int rc = -1;
 */

	rc = real_scandir(path, namelist, filter, compar);

/*	return rc;
 * }
 */
