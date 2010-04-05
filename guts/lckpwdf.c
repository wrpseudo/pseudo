/* 
 * Copyright (c) 2010 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * static int
 * wrap_lckpwdf(void) {
 *	int rc = -1;
 */
	rc = pseudo_pwd_lck_open();
	if (rc != -1) {
		struct flock lck = {
			.l_type = F_RDLCK,
			.l_whence = SEEK_SET,
			.l_start = 0,
			.l_len = 1
		};
		/* I don't really care whether this works. */
		fcntl(rc, F_SETFD, FD_CLOEXEC);
		/* Now lock it. */
		alarm(15); /* magic number from man page */
		rc = fcntl(rc, F_SETLKW, &lck);
		alarm(0);
		if (rc == -1) {
			int save_errno = errno;
			pseudo_pwd_lck_close();
			errno = save_errno;
		}
	}

/*	return rc;
 * }
 */
