/* 
 * static int
 * wrap_mkdir(const char *path, mode_t mode) {
 *	int rc = -1;
 */

	rc = wrap_mkdirat(AT_FDCWD, path, mode);

/*	return rc;
 * }
 */
