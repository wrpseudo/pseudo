/* 
 * Copyright (c) 2008-2010 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * static int
 * wrap_getresgid(gid_t *rgid, gid_t *egid, gid_t *sgid) {
 *	int rc = -1;
 */
	if (rgid)
		*rgid = pseudo_rgid;
	if (egid)
		*egid = pseudo_egid;
	if (sgid)
		*sgid = pseudo_sgid;
	if (rgid && egid && sgid) {
		rc = 0;
	} else {
		rc = -1;
		errno = EFAULT;
	}
/*	return rc;
 * }
 */
