/* 
 * static int
 * wrap_truncate(const char *path, off_t length) {
 *	int rc = -1;
 */

	rc = real_truncate(path, length);

/*	return rc;
 * }
 */
