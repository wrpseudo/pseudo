/* 
 * static int
 * wrap_rename(const char *oldpath, const char *newpath) {
 *	int rc = -1;
 */
 	pseudo_msg_t *msg;
 	struct stat64 oldbuf, newbuf;
	int oldrc, newrc;
	int save_errno;

	pseudo_debug(2, "rename: %s->%s\n",
		oldpath ? oldpath : "<nil>",
		newpath ? newpath : "<nil>");

	if (!oldpath || !newpath) {
		errno = EFAULT;
		return -1;
	}

	save_errno = errno;

	newrc = real___lxstat64(_STAT_VER, newpath, &newbuf);
	oldrc = real___lxstat64(_STAT_VER, oldpath, &oldbuf);

	errno = save_errno;

	rc = real_rename(oldpath, newpath);
	if (rc == -1) {
		/* we failed, and we don't care why */
		return rc;
	}
	save_errno = errno;
	/* nothing to do for a "rename" of a link to itself */
	if (newrc != -1 && oldrc != -1 &&
	    newbuf.st_dev == oldbuf.st_dev &&
	    newbuf.st_ino == oldbuf.st_ino) {
		return rc;
        }

	/* rename(3) is not mv(1).  rename(file, dir) fails; you must provide
	 * the corrected path yourself.  You can rename over a directory only
	 * if the source is a directory.  Symlinks are simply removed.
	 *
	 * If we got here, the real rename call succeeded.  That means newpath
	 * has been unlinked and oldpath has been linked to it.
	 *
	 * There are a ton of special cases to error check.  I don't check
	 * for any of them, because in every such case, the underlying rename
	 * failed, and there is nothing to do.
	 * The only tricky part is that, because we used to ignore symlinks,
	 * we may have to rename or remove directory trees even though in
	 * theory rename can never destroy a directory tree.
	 */

	/* newpath must be removed. */
	pseudo_client_op(OP_UNLINK, 0, -1, -1, newpath, &newbuf);

	/* fill in "correct" details from server */
	msg = pseudo_client_op(OP_STAT, 0, -1, -1, oldpath, &oldbuf);
	if (msg && msg->result == RESULT_SUCCEED) {
		pseudo_stat_msg(&oldbuf, msg);
		pseudo_debug(1, "renaming %s, got old mode of 0%o\n", oldpath, (int) msg->mode);
	} else {
		/* create an entry under the old name, which will then be
		 * renamed; this way, children would get renamed too, if there
		 * were any.
		 */
		pseudo_debug(1, "renaming new '%s' [%llu]\n",
			oldpath, (unsigned long long) oldbuf.st_ino);
		pseudo_client_op(OP_LINK, 0, -1, -1, oldpath, &oldbuf);
	}
	pseudo_client_op(OP_RENAME, 0, -1, -1, newpath, &oldbuf, oldpath);

	errno = save_errno;
/*	return rc;
 * }
 */
