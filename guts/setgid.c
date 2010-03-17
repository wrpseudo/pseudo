/* 
 * static int
 * wrap_setgid(gid_t gid) {
 *	int rc = -1;
 */
	if (pseudo_euid == 0) {
		pseudo_rgid = gid;
		pseudo_egid = gid;
		pseudo_sgid = gid;
		pseudo_fgid = gid;
		pseudo_client_touchgid();
		rc = 0;
	} else if (pseudo_egid == gid || pseudo_sgid == gid || pseudo_rgid == gid) {
		pseudo_egid = gid;
		pseudo_fgid = gid;
		pseudo_client_touchgid();
		rc = 0;
	} else {
		rc = -1;
		errno = EPERM;
	}
/*	return rc;
 * }
 */
