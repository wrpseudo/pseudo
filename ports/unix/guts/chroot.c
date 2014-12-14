/* 
 * Copyright (c) 2010 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * static int
 * wrap_chroot(const char *path) {
 *	int rc = -1;
 */
	pseudo_debug(PDBGF_CLIENT | PDBGF_CHROOT, "chroot: %s\n", path);
	if (!pseudo_client_op(OP_CHROOT, 0, -1, -1, path, 0)) {
		pseudo_debug(PDBGF_OP | PDBGF_CHROOT, "chroot failed: %s\n", strerror(errno));
		rc = -1;
	} else {
		rc = 0;
	}

/*	return rc;
 * }
 */
