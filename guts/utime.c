/* 
 * static int
 * wrap_utime(const char *path, const struct utimbuf *buf) {
 *	int rc = -1;
 */
	rc = real_utime(path, buf);

/*	return rc;
 * }
 */
