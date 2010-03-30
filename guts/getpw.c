/* 
 * Copyright (c) 2010 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * static int
 * wrap_getpw(uid_t uid, char *buf) {
 *	int rc = -1;
 */
	static struct passwd pwd;
	static char pwbuf[PSEUDO_PWD_MAX];
	struct passwd *pwp;

	pseudo_diag("warning: unsafe getpw() called.  hoping buf has at least %d chars.\n",
		PSEUDO_PWD_MAX);
	rc = wrap_getpwuid_r(uid, &pwd, pwbuf, PSEUDO_PWD_MAX, &pwp);
	/* different error return conventions */
	if (rc != 0) {
		errno = rc;
		rc = -1;
	} else {
		snprintf(buf, PSEUDO_PWD_MAX, "%s:%s:%d:%d:%s:%s:%s",
			pwd.pw_name,
			pwd.pw_passwd,
			pwd.pw_uid,
			pwd.pw_gid,
			pwd.pw_gecos,
			pwd.pw_dir,
			pwd.pw_shell);
	}

/*	return rc;
 * }
 */
