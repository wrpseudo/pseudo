/* 
 * Copyright (c) 2010 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * static int
 * wrap_getpwuid_r(uid_t uid, struct passwd *pwbuf, char *buf, size_t buflen, struct passwd **pwbufp) {
 *	int rc = -1;
 */

	setpwent();
	while ((rc = wrap_getpwent_r(pwbuf, buf, buflen, pwbufp)) == 0) {
		/* 0 means no error occurred, and *pwbufp == pwbuf */
		if (pwbuf->pw_uid == uid) {
			endpwent();
			return rc;
		}
	}
	endpwent();
	/* we never found a match; rc is 0 if there was no error, or
	 * non-zero if an error occurred.  Either way, set the
	 * pwbufp pointer to NULL to indicate that we didn't find
	 * something, and leave rc alone.
	 */
	*pwbufp = NULL;

/*	return rc;
 * }
 */
