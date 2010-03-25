/* 
 * static int
 * wrap_glob64(const char *pattern, int flags, int (*errfunc)(const char *, int), glob64_t *pglob) {
 *	int rc = -1;
 */

	rc = real_glob64(pattern, flags, errfunc, pglob);

/*	return rc;
 * }
 */
