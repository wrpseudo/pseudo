/* 
 * static uid_t
 * wrap_getuid(void) {
 *	uid_t rc = 0;
 */

	rc = pseudo_ruid;

/*	return rc;
 * }
 */
