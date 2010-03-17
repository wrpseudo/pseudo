/* 
 * static int
 * wrap___openat_2(int dirfd, const char *path, int flags) {
 *	int rc = -1;
 */

	rc = wrap_openat(dirfd, path, flags, 0);

/*	return rc;
 * }
 */
