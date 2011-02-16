/*
 * Copyright (c) 2011 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * int getgrgid_r(gid_t gid, struct group *gbuf, char *buf, size_t buflen, struct group **gbufp)
 *	int rc = -1;
 */

	rc = real_getgrgid_r(gid, gbuf, buf, buflen, gbufp);

/*	return rc;
 * }
 */
