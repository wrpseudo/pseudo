/*
 * Copyright (c) 2011 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * int getpwnam_r(const char *name, struct passwd *pwbuf, char *buf, size_t buflen, struct passwd **pwbufp)
 *	int rc = -1;
 */

	rc = real_getpwnam_r(name, pwbuf, buf, buflen, pwbufp);

/*	return rc;
 * }
 */
