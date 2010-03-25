/* 
 * static int
 * wrap_glob(const char *pattern, int flags, int (*errfunc)(const char *, int), glob_t *pglob) {
 *	int rc = -1;
 */

	rc = real_glob(pattern, flags, errfunc, pglob);

/*	return rc;
 * }
 */
