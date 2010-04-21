/* 
 * Copyright (c) 2008-2010 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * static int
 * wrap_unlinkat(int dirfd, const char *path, int rflags) {
 *	int rc = -1;
 */
	pseudo_msg_t *old_file;
	int save_errno;
	struct stat64 buf;

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
	rc = real___lxstat64(_STAT_VER, path, &buf);
#else
	rc = real___fxstatat64(_STAT_VER, dirfd, path, &buf, AT_SYMLINK_NOFOLLOW);
#endif
	if (rc == -1) {
		return rc;
	}
	old_file = pseudo_client_op(OP_UNLINK, 0, -1, dirfd, path, &buf);
#ifdef PSEUDO_NO_REAL_AT_FUNCTIONS
	rc = real_unlink(path);
#else
	rc = real_unlinkat(dirfd, path, rflags);
#endif
	if (rc == -1) {
		save_errno = errno;
		if (old_file && old_file->result == RESULT_SUCCEED) {
			pseudo_debug(1, "unlink failed, trying to relink...\n");
			buf.st_uid = old_file->uid;
			buf.st_gid = old_file->uid;
			buf.st_mode = old_file->mode;
			buf.st_dev = old_file->dev;
			buf.st_ino = old_file->ino;
			pseudo_client_op(OP_LINK, 0, -1, dirfd, path, &buf);
		} else {
			pseudo_debug(1, "unlink failed, but found no database entry, ignoring.\n");
		}
		errno = save_errno;
	}
/*	return rc;
 * }
 */
