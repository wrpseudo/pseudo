/* 
 * Copyright (c) 2010 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * static ssize_t
 * wrap_readlinkat(int dirfd, const char *path, char *buf, size_t bufsiz) {
 *	ssize_t rc = -1;
 */
	rc = real_readlinkat(dirfd, path, buf, bufsiz);

	if (rc > 0) {
		/* strip out a leading chrooted part */
		if (pseudo_chroot_len &&
			!memcmp(buf, pseudo_chroot, pseudo_chroot_len)) {
			if (buf[pseudo_chroot_len] == '/') {
				memmove(buf, buf + pseudo_chroot_len, rc - pseudo_chroot_len);
				rc -= pseudo_chroot_len;
			} else if (buf[pseudo_chroot_len] == '\0') {
				buf[0] = '/';
				rc = 1;
			}
			/* otherwise, it's not really a match... */
		}
	}

/*	return rc;
 * }
 */
