/* 
 * static int
 * wrap_lutimes(const char *path, const struct timeval *tv) {
 *	int rc = -1;
 */

	rc = real_lutimes(path, tv);

/*	return rc;
 * }
 */
