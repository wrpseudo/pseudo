/* 
 * Copyright (c) 2010-2011 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * static int
 * wrap_getgrent_r(struct group *gbuf, char *buf, size_t buflen, struct group **gbufp) {
 *	int rc = -1;
 */

	/* note that we don't wrap fgetgrent_r, since there's no path
	 * references in it.
	 */
	if (!pseudo_grp) {
		errno = ENOENT;
		return -1;
	}
	rc = fgetgrent_r(pseudo_grp, gbuf, buf, buflen, gbufp);

/*	return rc;
 * }
 */
