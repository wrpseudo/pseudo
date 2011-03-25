/*
 * Copyright (c) 2011 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * int fgetgrent_r(FILE *fp, struct group*gbuf, char *buf, size_t buflen, struct group **gbufp)
 *	int rc = -1;
 */

	rc = real_fgetgrent_r(fp, gbuf, buf, buflen, gbufp);

/*	return rc;
 * }
 */
