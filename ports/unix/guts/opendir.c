/* 
 * Copyright (c) 2010, 2012 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * static DIR *
 * wrap_opendir(const char *path) {
 *	DIR * rc = NULL;
 */
 	PSEUDO_STATBUF buf;
	int save_errno;

	rc = real_opendir(path);
	if (rc) {
		int fd;
		save_errno = errno;
		fd = dirfd(rc);
		if (base_fstat(fd, &buf) == -1) {
			pseudo_debug(PDBGF_CONSISTENCY, "diropen (fd %d) succeeded, but fstat failed (%s).\n",
				fd, strerror(errno));
			pseudo_client_op(OP_OPEN, PSA_READ, fd, -1, path, 0);
		} else {
			pseudo_client_op(OP_OPEN, PSA_READ, fd, -1, path, &buf);
		}


		errno = save_errno;
	}

/*	return rc;
 * }
 */
