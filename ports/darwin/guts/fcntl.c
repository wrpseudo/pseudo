/*
 * Copyright (c) 2011 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * int fcntl(int fd, int cmd, ... { struct flock *lock })
 *	int rc = -1;
 */
	int save_errno;
 	struct kludge { off_t unknown[6]; } x;

	va_start(ap, cmd);
	x = va_arg(ap, struct kludge);
	va_end(ap);

	rc = real_fcntl(fd, cmd, x);
	switch (cmd) {
	case F_DUPFD:
#ifdef F_DUPFD_CLOEXEC
	case F_DUPFD_CLOEXEC:
#endif
		/* actually do something */
		rc = real_fcntl(fd, cmd, x);
		save_errno = errno;
		if (rc != -1) {
			pseudo_debug(2, "fcntl_dup: %d->%d\n", fd, rc);
			pseudo_client_op(OP_DUP, 0, fd, rc, 0, 0);
		}
		errno = save_errno;
		break;
	default:
		/* pretty sure this is safe on x86. */
		rc = real_fcntl(fd, cmd, x);
		break;
	}

/*	return rc;
 * }
 */
