/*
 * Copyright (c) 2011 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * int getpwuid_r(uid_t uid, struct passwd *pwbuf, char *buf, size_t buflen, struct passwd **pwbufp)
 *	int rc = -1;
 */

	rc = real_getpwuid_r(uid, pwbuf, buf, buflen, pwbufp);

/*	return rc;
 * }
 */
