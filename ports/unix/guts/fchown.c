/* 
 * Copyright (c) 2008-2010, 2012, 2013 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * static int
 * wrap_fchown(int fd, uid_t owner, gid_t group) {
 *	int rc = -1;
 */
 	pseudo_msg_t *msg;
	PSEUDO_STATBUF buf;
	int save_errno = errno;

	if (base_fstat(fd, &buf) == -1) {
		save_errno = errno;
		pseudo_debug(PDBGF_CONSISTENCY, "fchown failing because fstat failed: %s\n",
			strerror(errno));
		errno = save_errno;
		return -1;
	}
	if (owner == (uid_t) -1 || group == (gid_t) -1) {
		msg = pseudo_client_op(OP_STAT, 0, fd, -1, NULL, &buf);
		/* copy in any existing values... */
		if (msg) {
			if (msg->result == RESULT_SUCCEED) {
				pseudo_stat_msg(&buf, msg);
			} else {
				pseudo_debug(PDBGF_FILE, "fchown fd %d, ino %llu, unknown file.\n",
					fd, (unsigned long long) buf.st_ino);
			}
		} else {
			pseudo_diag("stat within chown of fd %d [%llu] failed.\n",
				fd, (unsigned long long) buf.st_ino);
		}
	}
	/* now override with arguments */
	if (owner != (uid_t) -1) {
		buf.st_uid = owner;
	}
	if (group != (gid_t) -1) {
		buf.st_gid = group;
	}
	pseudo_debug(PDBGF_OP, "fchown, fd %d: %d:%d -> %d:%d\n",
		fd, owner, group, buf.st_uid, buf.st_gid);
	pseudo_client_op(OP_FCHOWN, 0, fd, -1, 0, &buf);
        /* pretend we worked, errno should be unchanged */
        errno = save_errno;
        rc = 0;

/*	return rc;
 * }
 */
