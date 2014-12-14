/* 
 * Copyright (c) 2008-2010, 2012, 2013 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * static int
 * wrap_fchmod(int fd, mode_t mode) {
 *	int rc = -1;
 */
	PSEUDO_STATBUF buf;
	int save_errno = errno;

	if (base_fstat(fd, &buf) == -1) {
		/* can't stat it, can't chmod it */
		return -1;
	}
	buf.st_mode = (buf.st_mode & ~07777) | (mode & 07777);
	pseudo_client_op(OP_FCHMOD, 0, fd, -1, 0, &buf);
	real_fchmod(fd, PSEUDO_FS_MODE(mode, S_ISDIR(buf.st_mode)));
        /* just pretend we worked */
        errno = save_errno;
        rc = 0;

/*	return rc;
 * }
 */
