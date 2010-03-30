/* 
 * Copyright (c) 2008-2010 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * static int
 * wrap_fchmod(int fd, mode_t mode) {
 *	int rc = -1;
 */
 	pseudo_msg_t *msg;
	struct stat64 buf;
	int save_errno = errno;

	if (real___fxstat64(_STAT_VER, fd, &buf) == -1) {
		/* can't stat it, can't chmod it */
		return -1;
	}
	buf.st_mode = (buf.st_mode & ~07777) | (mode & 07777);
	msg = pseudo_client_op(OP_FCHMOD, 0, fd, -1, 0, &buf);
	real_fchmod(fd, PSEUDO_FS_MODE(mode));
	if (!msg) {
		errno = ENOSYS;
		rc = -1;
	} else if (msg->result != RESULT_SUCCEED) {
		errno = EPERM;
		rc = -1;
	} else {
		errno = save_errno;
		rc = 0;
	}

/*	return rc;
 * }
 */
