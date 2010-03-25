/* 
 * static long
 * wrap_pathconf(const char *path, int name) {
 *	long rc = -1;
 */

	rc = real_pathconf(path, name);

/*	return rc;
 * }
 */
