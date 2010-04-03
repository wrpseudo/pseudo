/* 
 * Copyright (c) 2010 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * static int
 * wrap_glob64(const char *pattern, int flags, int (*errfunc)(const char *, int), glob64_t *pglob) {
 *	int rc = -1;
 */
	char *rpattern = NULL;
	int alloced = 0;

	/* note:  no canonicalization */
	if (pattern && (*pattern == '/') && pseudo_chroot_len) {
		size_t len = strlen(pattern) + pseudo_chroot_len + 2;
		rpattern = malloc(len);
		if (!rpattern) {
			errno = ENOMEM;
			return GLOB_NOSPACE;
		}
		snprintf(rpattern, len, "%s/%s", pseudo_chroot, pattern);
		alloced = 1;
	}

	rc = real_glob64(alloced ? rpattern : pattern, flags, errfunc, pglob);

	free(rpattern);

	if (rc == 0) {
		unsigned int i;
		for (i = 0; i < pglob->gl_pathc; ++i) {
			pseudo_dechroot(pglob->gl_pathv[i], (size_t) -1);
		}
	}
/*	return rc;
 * }
 */
