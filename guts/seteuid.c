/* 
 * Copyright (c) 2008-2010 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * static int
 * wrap_seteuid(uid_t euid) {
 *	int rc = -1;
 */
	if (pseudo_euid == 0 || euid == pseudo_euid || euid == pseudo_ruid || euid == pseudo_suid) {
		pseudo_euid = euid;
		pseudo_fuid = euid;
		pseudo_client_touchuid();
		rc = 0;
	} else {
		rc = -1;
		errno = EPERM;
	}
/*	return rc;
 * }
 */
