/* 
 * Copyright (c) 2008-2010 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * static int
 * wrap_rmdir(const char *path) {
 *	int rc = -1;
 */
	pseudo_msg_t *old_file;
 	struct stat64 buf;
	int save_errno;

	rc = real___lxstat64(_STAT_VER, path, &buf);
	if (rc == -1) {
		return rc;
	}
	old_file = pseudo_client_op(OP_UNLINK, 0, -1, -1, path, &buf);
	rc = real_rmdir(path);
	if (rc == -1) {
		save_errno = errno;
		if (old_file && old_file->result == RESULT_SUCCEED) {
			pseudo_debug(1, "rmdir failed, trying to relink...\n");
			buf.st_uid = old_file->uid;
			buf.st_gid = old_file->uid;
			buf.st_mode = old_file->mode;
			buf.st_dev = old_file->dev;
			buf.st_ino = old_file->ino;
			pseudo_client_op(OP_LINK, 0, -1, -1, path, &buf);
		} else {
			pseudo_debug(1, "rmdir failed, but found no database entry, ignoring.\n");
		}
		errno = save_errno;
	}

/*	return rc;
 * }
 */
