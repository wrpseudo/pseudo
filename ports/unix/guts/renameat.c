/* 
 * Copyright (c) 2008-2012 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * static int
 * wrap_renameat(int olddirfd, const char *oldpath, int newdirfd, const char *newpath) {
 *	int rc = -1;
 */
 	pseudo_msg_t *msg;
 	struct stat oldbuf, newbuf;
	int oldrc, newrc;
	int save_errno;
	int old_db_entry = 0;

	pseudo_debug(2, "renameat: %d,%s->%d,%s\n",
		olddirfd, oldpath ? oldpath : "<nil>",
		newdirfd, newpath ? newpath : "<nil>");

#ifdef PSEUDO_NO_REAL_AT_FUNCTIONS
	if (olddirfd != AT_FDCWD || newdirfd != AT_FDCWD) {
		errno = ENOSYS;
		return -1;
	}
#endif

	if (!oldpath || !newpath) {
		errno = EFAULT;
		return -1;
	}

	save_errno = errno;

#ifdef PSEUDO_NO_REAL_AT_FUNCTIONS
	newrc = real_lstat(newpath, &newbuf);
	oldrc = real_lstat(oldpath, &oldbuf);
#else
	oldrc = real___fxstatat(_STAT_VER, olddirfd, oldpath, &oldbuf, AT_SYMLINK_NOFOLLOW);
	newrc = real___fxstatat(_STAT_VER, newdirfd, newpath, &newbuf, AT_SYMLINK_NOFOLLOW);
#endif

	errno = save_errno;

	/* newpath must be removed. */
	/* as with unlink, we have to mark that the file may get deleted */
	msg = pseudo_client_op_plain(OP_MAY_UNLINK, 0, -1, newdirfd, newpath, newrc ? NULL : &newbuf);
	if (msg && msg->result == RESULT_SUCCEED)
		old_db_entry = 1;
	rc = real_renameat(olddirfd, oldpath, newdirfd, newpath);
	save_errno = errno;
	if (old_db_entry) {
		if (rc == -1) {
			/* since we failed, that wasn't really unlinked -- put
			 * it back.
			 */
			pseudo_client_op_plain(OP_CANCEL_UNLINK, 0, -1, newdirfd, newpath, &newbuf);
		} else {
			/* confirm that the file was removed */
			pseudo_client_op_plain(OP_DID_UNLINK, 0, -1, newdirfd, newpath, &newbuf);
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
		pseudo_debug(1, "creating new '%s' [%llu] to rename\n",
			oldpath, (unsigned long long) oldbuf.st_ino);
		pseudo_client_op_plain(OP_LINK, 0, -1, olddirfd, oldpath, &oldbuf);
	}
	/* special case: use 'fd' for olddirfd, because
	 * we know it has no other meaning for RENAME
	 */
	pseudo_client_op_plain(OP_RENAME, 0, olddirfd, newdirfd, newpath, &oldbuf, oldpath);

	errno = save_errno;
/*	return rc;
 * }
 */
