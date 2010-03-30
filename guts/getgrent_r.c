/* 
 * Copyright (c) 2010 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * static int
 * wrap_getgrent_r(struct group *gbuf, char *buf, size_t buflen, struct group **gbufp) {
 *	int rc = -1;
 */

	/* note that we don't wrap fgetgrent_r, since there's no path
	 * references in it.
	 */
	rc = fgetgrent_r(pseudo_grp, gbuf, buf, buflen, gbufp);
	if (rc == 0 && *gbufp) {
		if (*gbufp == gbuf) {
			pseudo_debug(1, "found group: %d/%s\n", gbuf->gr_gid, gbuf->gr_name);
		} else {
			pseudo_debug(1, "found group, but it's wrong?");
		}
	}

/*	return rc;
 * }
 */
