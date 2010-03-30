/* 
 * Copyright (c) 2008-2010 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * static FILE *
 * wrap_fopen(const char *path, const char *mode) {
 *	FILE * rc = 0;
 */
 	struct stat64 buf;
	int save_errno;
	int existed = (real___xstat64(_STAT_VER, path, &buf) != -1);

	rc = real_fopen(path, mode);
	save_errno = errno;

	if (rc) {
		int fd = fileno(rc);

		pseudo_debug(2, "fopen '%s': fd %d <FILE %p>\n", path, fd, (void *) rc);
		if (real___fxstat64(_STAT_VER, fd, &buf) != -1) {
			if (!existed) {
				pseudo_client_op(OP_CREAT, 0, -1, -1, path, &buf);
			}
			pseudo_client_op(OP_OPEN, pseudo_access_fopen(mode), fd, -1, path, &buf);
		} else {
			pseudo_debug(1, "fopen (fd %d) succeeded, but fstat failed (%s).\n",
				fd, strerror(errno));
			pseudo_client_op(OP_OPEN, pseudo_access_fopen(mode), fd, -1, path, 0);
		}
		errno = save_errno;
	}

/*	return rc;
 * }
 */
