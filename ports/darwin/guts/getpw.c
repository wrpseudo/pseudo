/*
 * Copyright (c) 2011 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * int getpw(uid_t uid, char *buf)
 *	int rc = -1;
 */

	rc = real_getpw(uid, buf);

/*	return rc;
 * }
 */
