/*
 * Copyright (c) 2011 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * int stat(const char *path, struct stat *buf)
 *	int rc = -1;
 */

	pseudo_msg_t *msg;
	int save_errno;

	rc = real_stat(path, buf);
	if (rc == -1) {
		return rc;
	}
	save_errno = errno;

	/* query database
	 * note that symlink canonicalizing is now automatic, so we
	 * don't need to check for a symlink on this end
	 */
	msg = pseudo_client_op(OP_STAT, 0, -1, AT_FDCWD, path, buf);
	if (msg) {
		pseudo_stat_msg(buf, msg);
	}

	errno = save_errno;

/*	return rc;
 * }
 */
