/* 
 * Copyright (c) 2008-2010 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * static FILE *
 * wrap_freopen64(const char *path, const char *mode, FILE *stream) {
 *	FILE * rc = NULL;
 */
 	struct stat64 buf;
	int save_errno;
	int existed = (real___xstat64(_STAT_VER, path, &buf) != -1);

	rc = real_freopen64(path, mode, stream);
	save_errno = errno;

	if (rc) {
		int fd = fileno(rc);

		pseudo_debug(PDBGF_FILE, "freopen64 '%s': fd %d\n", path, fd);
		if (real___fxstat64(_STAT_VER, fd, &buf) != -1) {
			if (!existed) {
				pseudo_client_op(OP_CREAT, 0, -1, -1, path, &buf);
			}
			pseudo_client_op(OP_OPEN, pseudo_access_fopen(mode), fd, -1, path, &buf);
		} else {
			pseudo_debug(PDBGF_FILE, "fopen (fd %d) succeeded, but stat failed (%s).\n",
				fd, strerror(errno));
			pseudo_client_op(OP_OPEN, pseudo_access_fopen(mode), fd, -1, path, 0);
		}
		errno = save_errno;
	}

/*	return rc;
 * }
 */
