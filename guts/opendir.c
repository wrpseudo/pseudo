/* 
 * static DIR *
 * wrap_opendir(const char *path) {
 *	DIR * rc = NULL;
 */

	rc = real_opendir(path);

/*	return rc;
 * }
 */
