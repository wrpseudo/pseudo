/* 
 * static int
 * wrap_utimes(const char *path, const struct timeval *times) {
 *	int rc = -1;
 */
	rc = real_utimes(path, times);

/*	return rc;
 * }
 */
