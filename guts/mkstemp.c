/* 
 * static int
 * wrap_mkstemp(char *template) {
 *	int rc = -1;
 */
	struct stat64 buf;
 	int save_errno;

	rc = real_mkstemp(template);

	if (rc != -1) {
		save_errno = errno;
		if (real___fxstat64(_STAT_VER, rc, &buf) != -1) {
			pseudo_client_op(OP_CREAT, 0, -1, -1, template, &buf);
			pseudo_client_op(OP_OPEN, 0, rc, -1, template, &buf);
		} else {
			pseudo_debug(1, "mkstemp (fd %d) succeeded, but fstat failed (%s).\n",
				rc, strerror(errno));
			pseudo_client_op(OP_OPEN, 0, rc, -1, template, 0);
		}
		errno = save_errno;
	}
/*	return rc;
 * }
 */
