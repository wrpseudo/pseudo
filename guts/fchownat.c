/* 
 * static int
 * wrap_fchownat(int dirfd, const char *path, uid_t owner, gid_t group, int flags) {
 *	int rc = -1;
 */
 	pseudo_msg_t *msg;
	struct stat64 buf;
	int save_errno;
	int doing_link = 0;

#ifdef PSEUDO_NO_REAL_AT_FUNCTIONS
	if (dirfd != AT_FDCWD) {
		errno = ENOSYS;
		return -1;
	}
	if (flags & AT_SYMLINK_NOFOLLOW) {
		rc = real___lxstat64(_STAT_VER, path, &buf);
	} else {
		rc = real___xstat64(_STAT_VER, path, &buf);
	}
#else
	rc = real___fxstatat64(_STAT_VER, dirfd, path, &buf, flags);
#endif
	if (rc == -1) {
		return rc;
	}
	/* pseudo won't track the ownership, here */
	if (S_ISLNK(buf.st_mode)) {
		doing_link = 1;
	}
	save_errno = errno;

	msg = pseudo_client_op(OP_STAT, flags, -1, -1, path, &buf);
	/* copy in any existing values... */
	if (msg) {
		if (msg->result == RESULT_SUCCEED) {
			pseudo_stat_msg(&buf, msg);
		} else {
			pseudo_debug(2, "chownat to %d:%d on %d/%s, ino %llu, new file.\n",
				owner, group, dirfd, path,
				(unsigned long long) buf.st_ino);
		}
	}
	/* now override with arguments */
	if (owner != -1) {
		buf.st_uid = owner;
	}
	if (group != -1) {
		buf.st_gid = group;
	}
	msg = pseudo_client_op(OP_CHOWN, flags, -1, dirfd, path, &buf);
	if (!msg) {
		errno = ENOSYS;
		rc = -1;
	} else if (msg->result != RESULT_SUCCEED) {
		errno = msg->xerrno;
		rc = -1;
	} else {
		rc = 0;
	}

/*	return rc;
 * }
 */
