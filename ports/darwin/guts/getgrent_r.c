/*
 * Copyright (c) 2011 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * int getgrent_r(struct group *gbuf, char *buf, size_t buflen, struct group **gbufp)
 *	int rc = -1;
 */

	rc = real_getgrent_r(gbuf, buf, buflen, gbufp);

/*	return rc;
 * }
 */
