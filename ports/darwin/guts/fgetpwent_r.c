/*
 * Copyright (c) 2011 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * int fgetpwent_r(FILE *fp, struct passwd *pwbuf, char *buf, size_t buflen, struct passwd **pwbufp)
 *	int rc = -1;
 */

	rc = real_fgetpwent_r(fp, pwbuf, buf, buflen, pwbufp);

/*	return rc;
 * }
 */
