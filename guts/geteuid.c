/* 
 * static uid_t
 * wrap_geteuid(void) {
 *	uid_t rc = 0;
 */

	rc = pseudo_euid;

/*	return rc;
 * }
 */
