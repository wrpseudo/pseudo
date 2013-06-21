/* 
 * Copyright (c) 2008-2010, 2012 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * static int
 * wrap_mkstemp(char *template) {
 *	int rc = -1;
 */
	PSEUDO_STATBUF buf;
 	int save_errno;
	size_t len;
	char *tmp_template;

	if (!template) {
		errno = EFAULT;
		return 0;
	}

	len = strlen(template);
	tmp_template = PSEUDO_ROOT_PATH(AT_FDCWD, template, AT_SYMLINK_NOFOLLOW);

	if (!tmp_template) {
		errno = ENOENT;
		return -1;
	}

	rc = real_mkstemp(tmp_template);

	if (rc != -1) {
		save_errno = errno;

		if (base_fstat(rc, &buf) != -1) {
			pseudo_client_op(OP_CREAT, 0, -1, -1, tmp_template, &buf);
			pseudo_client_op(OP_OPEN, PSA_READ | PSA_WRITE, rc, -1, tmp_template, &buf);
		} else {
			pseudo_debug(PDBGF_CONSISTENCY, "mkstemp (fd %d) succeeded, but fstat failed (%s).\n",
				rc, strerror(errno));
			pseudo_client_op(OP_OPEN, PSA_READ | PSA_WRITE, rc, -1, tmp_template, 0);
		}
		errno = save_errno;
	}
	/* mkstemp only changes the XXXXXX at the end. */
	memcpy(template + len - 6, tmp_template + strlen(tmp_template) - 6, 6);
	free(tmp_template);
/*	return rc;
 * }
 */
