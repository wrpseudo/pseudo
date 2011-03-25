/* 
 * Copyright (c) 2010-2011 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * static int
 * wrap_getpwent_r(struct passwd *pwbuf, char *buf, size_t buflen, struct passwd **pwbufp) {
 *	int rc = -1;
 */

	/* note that we don't wrap fgetpwent_r, since there's no path
	 * references in it.
	 */
	if (!pseudo_pwd) {
		errno = ENOENT;
		return -1;
	}
	rc = fgetpwent_r(pseudo_pwd, pwbuf, buf, buflen, pwbufp);

/*	return rc;
 * }
 */
