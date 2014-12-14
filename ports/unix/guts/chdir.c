/* 
 * Copyright (c) 2008-2010 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * static int
 * wrap_chdir(const char *path) {
 *	int rc = -1;
 */
	pseudo_debug(PDBGF_CLIENT, "chdir: '%s'\n",
		path ? path : "<nil>");

	if (!path) {
		errno = EFAULT;
		return -1;
	}
	rc = real_chdir(path);

	if (rc != -1) {
		pseudo_client_op(OP_CHDIR, 0, -1, -1, path, 0);
	}

/*	return rc;
 * }
 */
