/*
 * Copyright (c) 2011 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * int lstat(const char *path, struct stat *buf)
 *	int rc = -1;
 */

	pseudo_msg_t *msg;

	rc = real_lstat(path, buf);
	if (rc == -1) {
		return rc;
	}

	/* query database
	 * note that symlink canonicalizing is now automatic, so we
	 * don't need to check for a symlink on this end
	 */
	msg = pseudo_client_op(OP_STAT, 0, -1, -1, path, buf);
	if (msg && msg->result == RESULT_SUCCEED) {
		pseudo_stat_msg(buf, msg);
	}

/*	return rc;
 * }
 */
