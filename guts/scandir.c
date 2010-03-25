/* 
 * static int
 * wrap_scandir(const char *path, struct dirent ***namelist, int (*filter)(const struct dirent *), int (*compar)(const void *, const void *)) {
 *	int rc = -1;
 */

	rc = real_scandir(path, namelist, filter, compar);

/*	return rc;
 * }
 */
