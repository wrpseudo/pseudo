/* 
 * Copyright (c) 2008-2010 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * static int
 * wrap_link(const char *oldpath, const char *newpath) {
 *	int rc = -1;
 */
 	pseudo_msg_t *msg;
 	struct stat64 buf;

	rc = real_link(oldpath, newpath);
	if (rc == 0) {
		/* link(2) will not overwrite; if it succeeded, we know
		 * that there was no previous file with this name, so we
		 * shove it into the database.
		 */
		/* On linux, link(2) links to symlinks, not to the
		 * files they link to.  This is contraPOSIX, but
		 * it's apparently useful.
		 */
		real___lxstat64(_STAT_VER, oldpath, &buf);
		/* a link should copy the existing database entry, if
		 * there is one.  OP_LINK is also used to insert unseen
		 * files, though, so it can't be implicit.
		 */
		msg = pseudo_client_op(OP_STAT, 0, -1, -1, oldpath, &buf);
		if (msg) {
			pseudo_stat_msg(&buf, msg);
		}
		pseudo_client_op(OP_LINK, 0, -1, -1, newpath, &buf);
	}

/*	return rc;
 * }
 */
