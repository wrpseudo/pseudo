/* 
 * Copyright (c) 2008-2010 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
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

	/* newpath must be removed. */
	/* as with unlink, we have to do the remove before the operation
	 */
	msg = pseudo_client_op(OP_UNLINK, 0, -1, -1, newpath, newrc ? NULL : &newbuf);
	/* stash the server's old data */
	rc = real_rename(oldpath, newpath);
	save_errno = errno;
	if (rc == -1) {
		if (msg && msg->result == RESULT_SUCCEED) {
			newbuf.st_uid = msg->uid;
			newbuf.st_gid = msg->uid;
			newbuf.st_mode = msg->mode;
			newbuf.st_dev = msg->dev;
			newbuf.st_ino = msg->ino;
			/* since we failed, that wasn't really unlinked -- put
			 * it back.
			 */
			pseudo_client_op(OP_LINK, 0, -1, -1, newpath, &newbuf);
		}
		/* and we're done. */
		errno = save_errno;
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

	/* re-stat the new file.  Why?  Because if something got moved
	 * across device boundaries, its dev/ino changed!
	 */
	newrc = real___lxstat64(_STAT_VER, newpath, &newbuf);
	if (msg && msg->result == RESULT_SUCCEED) {
		pseudo_stat_msg(&oldbuf, msg);
		if (newrc == 0) {
			if (newbuf.st_dev != oldbuf.st_dev) {
				oldbuf.st_dev = newbuf.st_dev;
				oldbuf.st_ino = newbuf.st_ino;
			}
		}
		pseudo_debug(1, "renaming %s, got old mode of 0%o\n", oldpath, (int) msg->mode);
	} else {
		/* create an entry under the old name, which will then be
		 * renamed; this way, children would get renamed too, if there
		 * were any.
		 */
		if (newrc == 0) {
			if (newbuf.st_dev != oldbuf.st_dev) {
				oldbuf.st_dev = newbuf.st_dev;
				oldbuf.st_ino = newbuf.st_ino;
			}
		}
		pseudo_debug(1, "creating new '%s' [%llu] to rename\n",
			oldpath, (unsigned long long) oldbuf.st_ino);
		pseudo_client_op(OP_LINK, 0, -1, -1, oldpath, &oldbuf);
	}
	pseudo_client_op(OP_RENAME, 0, -1, -1, newpath, &oldbuf, oldpath);

	errno = save_errno;
/*	return rc;
 * }
 */
