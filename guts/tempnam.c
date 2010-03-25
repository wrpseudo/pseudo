/* 
 * static char *
 * wrap_tempnam(const char *template, const char *pfx) {
 *	char * rc = NULL;
 */
	pseudo_diag("tempnam() is so ludicrously insecure as to defy implementation.");
	errno = ENOMEM;
	rc = NULL;

/*	return rc;
 * }
 */
