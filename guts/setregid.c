/* 
 * static int
 * wrap_setregid(gid_t rgid, gid_t egid) {
 *	int rc = -1;
 */
	rc = 0;
	if (pseudo_euid != 0 && rgid != -1 &&
	    rgid != pseudo_egid && rgid != pseudo_rgid && rgid != pseudo_sgid) {
		rc = -1;
		errno = EPERM;
        }
	if (pseudo_euid != 0 && egid != -1 &&
	    egid != pseudo_egid && egid != pseudo_rgid && egid != pseudo_sgid) {
		rc = -1;
		errno = EPERM;
        }
	if (rc != -1) {
		if (rgid != -1)
			pseudo_rgid = rgid;
		if (egid != -1)
			pseudo_egid = egid;
		pseudo_fgid = pseudo_egid;
		pseudo_client_touchuid();
	}
/*	return rc;
 * }
 */
