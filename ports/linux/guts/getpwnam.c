/* 
 * Copyright (c) 2010 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * static struct passwd *
 * wrap_getpwnam(const char *name) {
 *	struct passwd * rc = NULL;
 */
	static struct passwd pwd;
	static char pwbuf[PSEUDO_PWD_MAX];
	int r_rc;

	r_rc = wrap_getpwnam_r(name, &pwd, pwbuf, PSEUDO_PWD_MAX, &rc);
	/* different error return conventions */
	if (r_rc != 0) {
		errno = r_rc;
	}

/*	return rc;
 * }
 */
