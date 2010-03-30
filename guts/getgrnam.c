/* 
 * Copyright (c) 2010 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * static struct group *
 * wrap_getgrnam(const char *name) {
 *	struct group * rc = NULL;
 */

	static struct group grp;
	static char grbuf[PSEUDO_PWD_MAX];
	int r_rc;

	r_rc = wrap_getgrnam_r(name, &grp, grbuf, PSEUDO_PWD_MAX, &rc);
	/* different error return conventions */
	if (r_rc != 0) {
		errno = r_rc;
	}


/*	return rc;
 * }
 */
