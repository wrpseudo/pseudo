/* 
 * Copyright (c) 2008-2010 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * static int
 * wrap_setfsgid(gid_t fsgid) {
 *	int rc = -1;
 */
	if (pseudo_euid == 0 ||
	    pseudo_egid == fsgid || pseudo_rgid == fsgid || pseudo_sgid == fsgid) {
		pseudo_fgid = fsgid;
		rc = 0;
	} else {
		rc = -1;
		errno = EPERM;
	}
/*	return rc;
 * }
 */
