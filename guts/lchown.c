/* 
 * static int
 * wrap_lchown(const char *path, uid_t owner, gid_t group) {
 */
 	pseudo_msg_t *msg;
	struct stat64 buf;

	pseudo_debug(2, "lchown(%s, %d, %d)\n",
		path ? path : "<null>", owner, group);
	if (real___lxstat64(_STAT_VER, path, &buf) == -1) {
		return -1;
	}
	if (owner == -1 || group == -1) {
		msg = pseudo_client_op(OP_STAT, AT_SYMLINK_NOFOLLOW, -1, -1, path, &buf);
		/* copy in any existing values... */
		if (msg) {
			if (msg->result == RESULT_SUCCEED) {
				pseudo_stat_msg(&buf, msg);
			} else {
				pseudo_debug(2, "lchown to %d:%d on %s, ino %llu, new file.\n",
					owner, group, path,
					(unsigned long long) buf.st_ino);
			}
		} else {
			pseudo_diag("stat within lchown of '%s' [%llu] failed.\n",
				path, (unsigned long long) buf.st_ino);
		}
	}
	if (owner != -1) {
		buf.st_uid = owner;
	}
	if (group != -1) {
		buf.st_gid = group;
	}
	msg = pseudo_client_op(OP_CHOWN, AT_SYMLINK_NOFOLLOW, -1, -1, path, &buf);
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
