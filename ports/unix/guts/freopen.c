/* 
 * Copyright (c) 2008-2010 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * static FILE *
 * wrap_freopen(const char *path, const char *mode, FILE *stream) {
 *	FILE * rc = NULL;
 */
 	PSEUDO_STATBUF buf;
	int save_errno;
	int existed = (base_stat(path, &buf) != -1);

	rc = real_freopen(path, mode, stream);
	save_errno = errno;

	if (rc) {
		int fd = fileno(rc);

		pseudo_debug(PDBGF_OP, "freopen '%s': fd %d\n", path, fd);
		if (base_fstat(fd, &buf) != -1) {
			if (!existed) {
				pseudo_client_op(OP_CREAT, 0, -1, -1, path, &buf);
			}
			pseudo_client_op(OP_OPEN, pseudo_access_fopen(mode), fd, -1, path, &buf);
		} else {
			pseudo_debug(PDBGF_OP, "fopen (fd %d) succeeded, but stat failed (%s).\n",
				fd, strerror(errno));
			pseudo_client_op(OP_OPEN, pseudo_access_fopen(mode), fd, -1, path, 0);
		}
		errno = save_errno;
	}

/*	return rc;
 * }
 */
