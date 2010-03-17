/* 
 * static int
 * wrap_setresuid(uid_t ruid, uid_t euid, uid_t suid) {
 *	int rc = -1;
 */
	rc = 0;
	if (pseudo_euid != 0 && ruid != -1 &&
	    ruid != pseudo_euid && ruid != pseudo_ruid && ruid != pseudo_suid) {
		rc = -1;
		errno = EPERM;
        }
	if (pseudo_euid != 0 && euid != -1 &&
	    euid != pseudo_euid && euid != pseudo_ruid && euid != pseudo_suid) {
		rc = -1;
		errno = EPERM;
        }
	if (pseudo_euid != 0 && suid != -1 &&
	    suid != pseudo_euid && suid != pseudo_ruid && suid != pseudo_suid) {
		rc = -1;
		errno = EPERM;
        }
	if (rc != -1) {
		if (ruid != -1)
			pseudo_ruid = ruid;
		if (euid != -1)
			pseudo_euid = euid;
		if (suid != -1)
			pseudo_suid = suid;
		pseudo_fuid = pseudo_euid;
		pseudo_client_touchuid();
	}
/*	return rc;
 * }
 */
