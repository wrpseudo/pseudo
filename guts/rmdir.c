/* 
 * static int
 * wrap_rmdir(const char *path) {
 *	int rc = -1;
 */
 	struct stat64 buf;
	int save_errno;

	rc = real___lxstat64(_STAT_VER, path, &buf);
	if (rc == -1) {
		return rc;
	}
	rc = real_rmdir(path);
	save_errno = errno;
	if (rc != -1) {
		pseudo_client_op(OP_UNLINK, 0, -1, -1, path, &buf);
	}

	errno = save_errno;
/*	return rc;
 * }
 */
