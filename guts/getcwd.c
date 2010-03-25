/* 
 * static char *
 * wrap_getcwd(char *buf, size_t size) {
 *	char * rc = NULL;
 */
	pseudo_debug(2, "wrap_getcwd: %p, %lu\n",
		(void *) buf, (unsigned long) size);
	if (!pseudo_cwd) {
		pseudo_diag("Asked for CWD, but don't have it!\n");
		errno = EACCES;
		return NULL;
	}
	/* emulate Linux semantics in case of non-Linux systems. */
	if (!buf) {
		/* if we don't have one, something's very wrong... */
		if (!size) {
			size = pseudo_cwd_len;
		}
		if (size) {
			buf = malloc(size);
		} else {
			pseudo_diag("can't figure out CWD: length %ld\n",
				(unsigned long) pseudo_cwd_len);
		}
		if (!buf) {
			pseudo_diag("couldn't allocate requested CWD buffer - need %ld byes\n",
				(unsigned long) size);
			errno = ENOMEM;
			return NULL;
		}
	}
	rc = buf;
	memcpy(buf, pseudo_cwd, pseudo_cwd_len + 1);
	if (!*buf) {
		strcpy(buf, "/");
	}

/*	return rc;
 * }
 */
