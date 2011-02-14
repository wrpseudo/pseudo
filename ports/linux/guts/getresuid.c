/* 
 * Copyright (c) 2008-2010 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * static int
 * wrap_getresuid(uid_t *ruid, uid_t *euid, uid_t *suid) {
 *	int rc = -1;
 */
	if (ruid)
		*ruid = pseudo_ruid;
	if (euid)
		*euid = pseudo_euid;
	if (suid)
		*suid = pseudo_suid;
	if (ruid && euid && suid) {
		rc = 0;
	} else {
		rc = -1;
		errno = EFAULT;
	}
/*	return rc;
 * }
 */
