/* 
 * static int
 * wrap_symlink(const char *oldpath, const char *newpath) {
 *	int rc = -1;
 */

	rc = wrap_symlinkat(oldpath, AT_FDCWD, newpath);

/*	return rc;
 * }
 */
