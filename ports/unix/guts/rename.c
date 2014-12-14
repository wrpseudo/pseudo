/* 
 * Copyright (c) 2008-2010, 2012 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * static int
 * wrap_rename(const char *oldpath, const char *newpath) {
 *	int rc = -1;
 */
 	pseudo_msg_t *msg;
 	PSEUDO_STATBUF oldbuf, newbuf;
	int oldrc, newrc;
	int save_errno;
	int old_db_entry = 0;
	int may_unlinked = 0;

	pseudo_debug(PDBGF_OP, "rename: %s->%s\n",
		oldpath ? oldpath : "<nil>",
		newpath ? newpath : "<nil>");

	if (!oldpath || !newpath) {
		errno = EFAULT;
		return -1;
	}

	save_errno = errno;

	newrc = base_lstat(newpath, &newbuf);
	oldrc = base_lstat(oldpath, &oldbuf);

	errno = save_errno;

	/* newpath must be removed. */
	/* as with unlink, we have to mark that the file may get deleted */
	msg = pseudo_client_op(OP_MAY_UNLINK, 0, -1, -1, newpath, newrc ? NULL : &newbuf);
	if (msg && msg->result == RESULT_SUCCEED)
		may_unlinked = 1;
	msg = pseudo_client_op(OP_STAT, 0, -1, -1, oldpath, oldrc ? NULL : &oldbuf);
	if (msg && msg->result == RESULT_SUCCEED)
		old_db_entry = 1;
	rc = real_rename(oldpath, newpath);
	save_errno = errno;
	if (may_unlinked) {
		if (rc == -1) {
			/* since we failed, that wasn't really unlinked -- put
			 * it back.
			 */
			pseudo_client_op(OP_CANCEL_UNLINK, 0, -1, -1, newpath, &newbuf);
		} else {
			/* confirm that the file was removed */
			pseudo_client_op(OP_DID_UNLINK, 0, -1, -1, newpath, &newbuf);
		}
	}
	if (rc == -1) {
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
	if (!old_db_entry) {
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
		pseudo_debug(PDBGF_FILE, "creating new '%s' [%llu] to rename\n",
			oldpath, (unsigned long long) oldbuf.st_ino);
		pseudo_client_op(OP_LINK, 0, -1, -1, oldpath, &oldbuf);
	}
	pseudo_client_op(OP_RENAME, 0, -1, -1, newpath, &oldbuf, oldpath);

	errno = save_errno;
/*	return rc;
 * }
 */
