/* 
 * static int
 * wrap_getgroups(int size, gid_t *list) {
 *	int rc = -1;
 */

	/* you're only in group zero */
	rc = 1;
	if (size > 0) {
		list[0] = 0;
	}

/*	return rc;
 * }
 */
