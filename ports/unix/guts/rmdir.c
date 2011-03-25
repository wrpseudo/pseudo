/* 
 * Copyright (c) 2008-2010 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * static int
 * wrap_rmdir(const char *path) {
 *	int rc = -1;
 */
	pseudo_msg_t *msg;
 	struct stat buf;
	int save_errno;
	int old_db_entry = 0;

	rc = real_lstat(path, &buf);
	if (rc == -1) {
		return rc;
	}
	msg = pseudo_client_op_plain(OP_MAY_UNLINK, 0, -1, -1, path, &buf);
	if (msg && msg->result == RESULT_SUCCEED)
		old_db_entry = 1;
	rc = real_rmdir(path);
	if (old_db_entry) {
		if (rc == -1) {
			save_errno = errno;
			pseudo_client_op_plain(OP_CANCEL_UNLINK, 0, -1, -1, path, &buf);
			errno = save_errno;
		} else {
			pseudo_client_op_plain(OP_DID_UNLINK, 0, -1, -1, path, &buf);
		}
	} else {
		pseudo_debug(1, "rmdir on <%s>, not in database, no effect.\n", path);
	}

/*	return rc;
 * }
 */
