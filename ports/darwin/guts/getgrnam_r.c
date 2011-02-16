/*
 * Copyright (c) 2011 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * int getgrnam_r(const char *name, struct group *gbuf, char *buf, size_t buflen, struct group **gbufp)
 *	int rc = -1;
 */

	rc = real_getgrnam_r(name, gbuf, buf, buflen, gbufp);

/*	return rc;
 * }
 */
