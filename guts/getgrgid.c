/* 
 * Copyright (c) 2010 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * static struct group *
 * wrap_getgrgid(gid_t gid) {
 *	struct group * rc = NULL;
 */
	static struct group grp;
	static char grbuf[PSEUDO_PWD_MAX];
	int r_rc;

	r_rc = wrap_getgrgid_r(gid, &grp, grbuf, PSEUDO_PWD_MAX, &rc);
	/* different error return conventions */
	if (r_rc != 0) {
		errno = r_rc;
	}

/*	return rc;
 * }
 */
