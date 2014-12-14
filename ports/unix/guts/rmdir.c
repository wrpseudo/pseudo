/* 
 * Copyright (c) 2008-2010, 2012 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * static int
 * wrap_rmdir(const char *path) {
 *	int rc = -1;
 */
	pseudo_msg_t *msg;
 	PSEUDO_STATBUF buf;
	int save_errno;
	int old_db_entry = 0;

	rc = base_lstat(path, &buf);
	if (rc == -1) {
		return rc;
	}
	msg = pseudo_client_op(OP_MAY_UNLINK, 0, -1, -1, path, &buf);
	if (msg && msg->result == RESULT_SUCCEED)
		old_db_entry = 1;
	rc = real_rmdir(path);
	if (old_db_entry) {
		if (rc == -1) {
			save_errno = errno;
			pseudo_client_op(OP_CANCEL_UNLINK, 0, -1, -1, path, &buf);
			errno = save_errno;
		} else {
			pseudo_client_op(OP_DID_UNLINK, 0, -1, -1, path, &buf);
		}
	} else {
		pseudo_debug(PDBGF_FILE, "rmdir on <%s>, not in database, no effect.\n", path);
	}

/*	return rc;
 * }
 */
