/* 
 * Copyright (c) 2010 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * static DIR *
 * wrap_opendir(const char *path) {
 *	DIR * rc = NULL;
 */
 	struct stat buf;
	int save_errno;

	rc = real_opendir(path);
	if (rc) {
		int fd;
		save_errno = errno;
		fd = dirfd(rc);
		if (real_fstat(fd, &buf) == -1) {
			pseudo_debug(1, "diropen (fd %d) succeeded, but fstat failed (%s).\n",
				fd, strerror(errno));
			pseudo_client_op_plain(OP_OPEN, PSA_READ, fd, -1, path, 0);
		} else {
			pseudo_client_op_plain(OP_OPEN, PSA_READ, fd, -1, path, &buf);
		}


		errno = save_errno;
	}

/*	return rc;
 * }
 */
