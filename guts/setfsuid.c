/* 
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
