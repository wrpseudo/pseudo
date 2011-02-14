/*
 * Copyright (c) 2011 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * int getpwent_r(struct passwd *pwbuf, char *buf, size_t buflen, struct passwd **pwbufp)
 *	int rc = -1;
 */

	rc = real_getpwent_r(pwbuf, buf, buflen, pwbufp);

/*	return rc;
 * }
 */
