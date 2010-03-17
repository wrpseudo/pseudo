/* 
 * static int
 * wrap_creat(const char *path, mode_t mode) {
 *	int rc = -1;
 */

	rc = wrap_openat(AT_FDCWD, path, O_CREAT|O_WRONLY|O_TRUNC, mode);

/*	return rc;
 * }
 */
