/* 
 * static int
 * wrap_chdir(const char *path) {
 *	int rc = -1;
 */
	pseudo_debug(3, "chdir: %s\n", path ? path : "<nil>");
	rc = real_chdir(path);

	if (rc != -1) {
		pseudo_client_op(OP_CHDIR, 0, -1, -1, path, 0);
	}

/*	return rc;
 * }
 */
