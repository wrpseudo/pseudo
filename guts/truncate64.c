/* 
 * static int
 * wrap_truncate64(const char *path, off64_t length) {
 *	int rc = -1;
 */

	rc = real_truncate64(path, length);

/*	return rc;
 * }
 */
