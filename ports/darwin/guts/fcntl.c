/*
 * Copyright (c) 2011, 2012 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * int fcntl(int fd, int cmd, ... { struct flock *lock })
 *	int rc = -1;
 */
	int save_errno;
	long long flag = 0;

	va_start(ap, cmd);
	flag = va_arg(ap, long long);
	va_end(ap);
	rc = real_fcntl(fd, cmd, flag);

	switch (cmd) {
	case F_DUPFD:
#ifdef F_DUPFD_CLOEXEC
	/* it doesn't exist now, but if I take this out they'll add it
	 * just to mess with me.
	 */
	case F_DUPFD_CLOEXEC:
#endif
		/* actually do something */
		save_errno = errno;
		if (rc != -1) {
			pseudo_debug(2, "fcntl_dup: %d->%d\n", fd, rc);
			pseudo_client_op(OP_DUP, 0, fd, rc, 0, 0);
		}
		errno = save_errno;
		break;
	default:
		/* nothing to do, we hope */
		break;
	}

	save_errno = errno;
	pseudo_debug(3, "fcntl(fd %d, cmd %d, %llx) => %d (%s)\n",
		fd, cmd, flag, rc, strerror(errno));
	errno = save_errno;

/*	return rc;
 * }
 */
