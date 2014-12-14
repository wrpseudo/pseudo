/* 
 * Copyright (c) 2008-2010 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * static char *
 * wrap_getcwd(char *buf, size_t size) {
 *	char * rc = NULL;
 */
	pseudo_debug(PDBGF_CLIENT, "wrap_getcwd: %p, %lu\n",
		(void *) buf, (unsigned long) size);
	if (!pseudo_cwd) {
		pseudo_diag("Asked for CWD, but don't have it!\n");
		errno = EACCES;
		return NULL;
	}
	/* emulate Linux semantics in case of non-Linux systems. */
	if (!buf) {
		/* if we don't have a cwd, something's very wrong... */
		if (!size) {
			size = pseudo_cwd_len + 1;
			if (pseudo_chroot_len && size >= pseudo_chroot_len &&
				!memcmp(pseudo_cwd, pseudo_chroot, pseudo_chroot_len)) {
				size -= pseudo_chroot_len;	
				/* if cwd is precisely the same as chroot, we
				 * actually want a /, not an empty string
				 */
				if (size < 2)
					size = 2;
			}
		}
		if (size) {
			buf = malloc(size);
		} else {
			pseudo_diag("can't figure out CWD: length %ld + 1 - %ld => %ld\n",
				(unsigned long) pseudo_cwd_len,
				(unsigned long) pseudo_chroot_len,
				(unsigned long) size);
		}
		if (!buf) {
			pseudo_diag("couldn't allocate requested CWD buffer - need %ld byes\n",
				(unsigned long) size);
			errno = ENOMEM;
			return NULL;
		}
	}
	if (pseudo_cwd_len - (pseudo_cwd_rel - pseudo_cwd) >= size) {
		pseudo_debug(PDBGF_CLIENT, "only %ld bytes available, need %ld (%ld + 1 - %ld)\n",
			(unsigned long) size,
			(unsigned long) pseudo_cwd_len + 1 - pseudo_chroot_len,
			(unsigned long) pseudo_cwd_len,
			(unsigned long) pseudo_chroot_len);
		errno = ERANGE;
		return NULL;
	}
	rc = buf;
	pseudo_debug(PDBGF_CLIENT, "getcwd: copying %d (%d + 1 - %d) characters from <%s>.\n",
		(int) ((pseudo_cwd_len + 1) - pseudo_chroot_len),
		(int) pseudo_cwd_len, (int) pseudo_chroot_len,
		pseudo_cwd_rel);
	memcpy(buf, pseudo_cwd_rel, (pseudo_cwd_len + 1) - (pseudo_cwd_rel - pseudo_cwd));
	if (!*buf) {
		strcpy(buf, "/");
	}

/*	return rc;
 * }
 */
