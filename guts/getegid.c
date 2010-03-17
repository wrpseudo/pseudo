/* 
 * static gid_t
 * wrap_getegid(void) {
 *	gid_t rc = 0;
 */

	rc = pseudo_egid;

/*	return rc;
 * }
 */
