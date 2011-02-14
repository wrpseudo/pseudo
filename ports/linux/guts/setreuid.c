/* 
 * Copyright (c) 2008-2010 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * static int
 * wrap_setreuid(uid_t ruid, uid_t euid) {
 *	int rc = -1;
 */
	rc = 0;
	if (pseudo_euid != 0 && ruid != (uid_t) -1 &&
	    ruid != pseudo_euid && ruid != pseudo_ruid && ruid != pseudo_suid) {
		rc = -1;
		errno = EPERM;
        }
	if (pseudo_euid != 0 && euid != (uid_t) -1 &&
	    euid != pseudo_euid && euid != pseudo_ruid && euid != pseudo_suid) {
		rc = -1;
		errno = EPERM;
        }
	if (rc != -1) {
		if (ruid != (uid_t) -1)
			pseudo_ruid = ruid;
		if (euid != (uid_t) -1)
			pseudo_euid = euid;
		pseudo_fuid = pseudo_euid;
		pseudo_client_touchuid();
	}
/*	return rc;
 * }
 */
