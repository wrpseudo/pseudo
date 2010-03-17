/* 
 * static int
 * wrap_unlink(const char *path) {
 *	int rc = -1;
 */

	rc = wrap_unlinkat(AT_FDCWD, path, 0);

/*	return rc;
 * }
 */
