/* 
 * Copyright (c) 2008-2010 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * static int
 * wrap_setregid(gid_t rgid, gid_t egid) {
 *	int rc = -1;
 */
	rc = 0;
	if (pseudo_euid != 0 && rgid != (gid_t) -1 &&
	    rgid != pseudo_egid && rgid != pseudo_rgid && rgid != pseudo_sgid) {
		rc = -1;
		errno = EPERM;
        }
	if (pseudo_euid != 0 && egid != (gid_t) -1 &&
	    egid != pseudo_egid && egid != pseudo_rgid && egid != pseudo_sgid) {
		rc = -1;
		errno = EPERM;
        }
	if (rc != -1) {
		if (rgid != (gid_t) -1)
			pseudo_rgid = rgid;
		if (egid != (gid_t) -1)
			pseudo_egid = egid;
		pseudo_fgid = pseudo_egid;
		pseudo_client_touchuid();
	}
/*	return rc;
 * }
 */
