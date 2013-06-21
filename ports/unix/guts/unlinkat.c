/* 
 * Copyright (c) 2008-2010, 2012 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * static int
 * wrap_unlinkat(int dirfd, const char *path, int rflags) {
 *	int rc = -1;
 */
	pseudo_msg_t *msg;
	int save_errno;
	PSEUDO_STATBUF buf;
	int old_db_entry = 0;

#ifdef PSEUDO_NO_REAL_AT_FUNCTIONS
	if (dirfd != AT_FDCWD) {
		errno = ENOSYS;
		return -1;
	}
	if (rflags) {
		/* the only supported flag is AT_REMOVEDIR.  We'd never call
		 * with that flag unless the real AT functions exist, so 
		 * something must have gone horribly wrong....
		 */
		pseudo_diag("wrap_unlinkat called with flags (0x%x), path '%s'\n",
			rflags, path ? path : "<nil>");
		errno = ENOSYS;
		return -1;
	}
#endif

#ifdef PSEUDO_NO_REAL_AT_FUNCTIONS
	rc = base_lstat(path, &buf);
#else
	rc = base_fstatat(dirfd, path, &buf, AT_SYMLINK_NOFOLLOW);
#endif
	if (rc == -1) {
		return rc;
	}
	msg = pseudo_client_op(OP_MAY_UNLINK, 0, -1, dirfd, path, &buf);
	if (msg && msg->result == RESULT_SUCCEED)
		old_db_entry = 1;
#ifdef PSEUDO_NO_REAL_AT_FUNCTIONS
	rc = real_unlink(path);
#else
	rc = real_unlinkat(dirfd, path, rflags);
#endif
	if (old_db_entry) {
		if (rc == -1) {
			save_errno = errno;
			pseudo_client_op(OP_CANCEL_UNLINK, 0, -1, -1, path, &buf);
			errno = save_errno;
		} else {
			pseudo_client_op(OP_DID_UNLINK, 0, -1, -1, path, &buf);
		}
	} else {
		pseudo_debug(PDBGF_FILE, "unlink on <%s>, not in database, no effect.\n", path);
	}

/*	return rc;
 * }
 */
