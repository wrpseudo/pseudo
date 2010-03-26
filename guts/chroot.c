/* 
 * static int
 * wrap_chroot(const char *path) {
 *	int rc = -1;
 */
	pseudo_debug(2, "chroot: %s\n", path);
	if (!pseudo_client_op(OP_CHROOT, 0, -1, -1, path, 0)) {
		pseudo_debug(1, "chroot failed: %s\n", strerror(errno));
		rc = -1;
	} else {
		rc = 0;
	}

/*	return rc;
 * }
 */
