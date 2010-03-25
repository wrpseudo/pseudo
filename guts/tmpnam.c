/* 
 * static char *
 * wrap_tmpnam(char *s) {
 *	char * rc = NULL;
 */

	pseudo_diag("tmpnam() is so ludicrously insecure as to defy implementation.");
	errno = ENOMEM;
	rc = NULL;

/*	return rc;
 * }
 */
