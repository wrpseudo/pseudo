/* 
 * Copyright (c) 2010 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * static int
 * wrap_scandir64(const char *path, struct dirent64 ***namelist, int (*filter)(const struct dirent64 *), int (*compar)(const void *, const void *)) {
 *	int rc = -1;
 */

	rc = real_scandir64(path, namelist, filter, compar);

/*	return rc;
 * }
 */
