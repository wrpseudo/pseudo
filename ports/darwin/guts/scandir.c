/*
 * Copyright (c) 2011 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * int scandir(const char *path, struct dirent ***namelist, int (*filter)(const struct dirent *), int (*compar)())
 *	int rc = -1;
 */

	rc = real_scandir(path, namelist, filter, compar);

/*	return rc;
 * }
 */
