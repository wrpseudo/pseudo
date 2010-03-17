/* 
 * static int
 * wrap___openat64_2(int dirfd, const char *path, int flags) {
 *	int rc = -1;
 */

	rc = wrap_openat(dirfd, path, flags, O_LARGEFILE);

/*	return rc;
 * }
 */
