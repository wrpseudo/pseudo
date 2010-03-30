/* 
 * Copyright (c) 2008-2010 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * static int
 * wrap_setuid(uid_t uid) {
 *	int rc = -1;
 */
	if (pseudo_euid == 0) {
		pseudo_ruid = uid;
		pseudo_euid = uid;
		pseudo_suid = uid;
		pseudo_fuid = uid;
		pseudo_client_touchuid();
		rc = 0;
	} else if (pseudo_euid == uid || pseudo_suid == uid || pseudo_ruid == uid) {
		pseudo_euid = uid;
		pseudo_fuid = uid;
		pseudo_client_touchuid();
		rc = 0;
	} else {
		rc = -1;
		errno = EPERM;
	}
/*	return rc;
 * }
 */
