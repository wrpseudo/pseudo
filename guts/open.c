/* 
 * static int
 * wrap_open(const char *path, int flags, ...mode_t mode) {
 *	int rc = -1;
 */

	return wrap_openat(AT_FDCWD, path, flags, mode);

/*	return rc;
 * }
 */
