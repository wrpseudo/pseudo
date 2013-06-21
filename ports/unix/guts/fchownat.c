/* 
 * Copyright (c) 2008-2010, 2012, 2013 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * static int
 * wrap_fchownat(int dirfd, const char *path, uid_t owner, gid_t group, int flags) {
 *	int rc = -1;
 */
 	pseudo_msg_t *msg;
	PSEUDO_STATBUF buf;
	int save_errno = errno;
	int doing_link = 0;

#ifdef PSEUDO_NO_REAL_AT_FUNCTIONS
	if (dirfd != AT_FDCWD) {
		errno = ENOSYS;
		return -1;
	}
	if (flags & AT_SYMLINK_NOFOLLOW) {
		rc = base_lstat(path, &buf);
	} else {
		rc = base_stat(path, &buf);
	}
#else
	rc = base_fstatat(dirfd, path, &buf, flags);
#endif
	if (rc == -1) {
		return rc;
	}
	/* pseudo won't track the ownership, here */
	if (S_ISLNK(buf.st_mode)) {
		doing_link = 1;
	}
	save_errno = errno;

	if (owner == (uid_t) -1 || group == (gid_t) -1) {
		msg = pseudo_client_op(OP_STAT, 0, -1, -1, path, &buf);
		/* copy in any existing values... */
		if (msg) {
			if (msg->result == RESULT_SUCCEED) {
				pseudo_stat_msg(&buf, msg);
			} else {
				pseudo_debug(PDBGF_FILE, "chownat to %d:%d on %d/%s, ino %llu, new file.\n",
					owner, group, dirfd, path,
					(unsigned long long) buf.st_ino);
			}
		}
	}
	/* now override with arguments */
	if (owner != (uid_t) -1) {
		buf.st_uid = owner;
	}
	if (group != (gid_t) -1) {
		buf.st_gid = group;
	}
	pseudo_client_op(OP_CHOWN, 0, -1, dirfd, path, &buf);
        /* just pretend we worked */
        errno = save_errno;
        rc = 0;

/*	return rc;
 * }
 */
