/* 
 * static int
 * wrap_symlink(const char *oldname, const char *newpath) {
 *	int rc = -1;
 */

	rc = wrap_symlinkat(oldname, AT_FDCWD, newpath);

/*	return rc;
 * }
 */
