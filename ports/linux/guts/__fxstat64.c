/* 
 * Copyright (c) 2008-2010 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * int
 * wrap___fxstat64(int ver, int fd, struct stat64 *buf) {
 *	int rc = -1;
 */
	pseudo_msg_t *msg;
	int save_errno;

	rc = real___fxstat64(ver, fd, buf);
	save_errno = errno;
	if (rc == -1) {
		return rc;
	}
	if (ver != _STAT_VER) {
		pseudo_debug(PDBGF_CLIENT, "version mismatch: got stat version %d, only supporting %d\n", ver, _STAT_VER);
		errno = save_errno;
		return rc;
	}
	msg = pseudo_client_op(OP_FSTAT, 0, fd, -1, 0, buf);
	if (msg && msg->result == RESULT_SUCCEED) {
		pseudo_stat_msg(buf, msg);
	}

	errno = save_errno;
/*	return rc;
 * }
 */
