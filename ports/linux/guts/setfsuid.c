/* 
 * Copyright (c) 2008-2010 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * static int
 * wrap_setfsuid(uid_t fsuid) {
 *	int rc = -1;
 */
	if (pseudo_euid == 0 ||
	    pseudo_euid == fsuid || pseudo_ruid == fsuid || pseudo_suid == fsuid) {
		pseudo_fuid = fsuid;
		rc = 0;
	} else {
		rc = -1;
		errno = EPERM;
	}
/*	return rc;
 * }
 */
