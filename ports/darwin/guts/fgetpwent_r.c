/*
 * Copyright (c) 2011 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * int fgetpwent_r(FILE *fp, struct passwd *pbuf, char *buf, size_t buflen, struct passwd **pbufp)
 *	int rc = -1;
 */

	rc = real_fgetpwent_r(fp, pbuf, buf, buflen, pbufp);

/*	return rc;
 * }
 */
