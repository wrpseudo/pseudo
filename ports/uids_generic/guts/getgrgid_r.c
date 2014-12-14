/* 
 * Copyright (c) 2010 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * static int
 * wrap_getgrgid_r(gid_t gid, struct group *gbuf, char *buf, size_t buflen, struct group **gbufp) {
 *	int rc = -1;
 */

	setgrent();
	while ((rc = wrap_getgrent_r(gbuf, buf, buflen, gbufp)) == 0) {
		/* 0 means no error occurred, and *gbufp == gbuf */
		if (gbuf->gr_gid == gid) {
			pseudo_debug(PDBGF_CLIENT, "found group gid %d, name %s\n",
				gbuf->gr_gid, gbuf->gr_name);
			endgrent();
			return rc;
		}
	}
	endgrent();
	/* we never found a match; rc is 0 if there was no error, or
	 * non-zero if an error occurred.  Either way, set the
	 * pwbufp pointer to NULL to indicate that we didn't find
	 * something, and leave rc alone.
	 */
	*gbufp = NULL;

/*	return rc;
 * }
 */
