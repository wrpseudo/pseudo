/* 
 * Copyright (c) 2010, 2012 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * static char *
 * wrap_mkdtemp(char *template) {
 *	char * rc = NULL;
 */
	PSEUDO_STATBUF buf;
 	int save_errno;
	size_t len;
	char *tmp_template;

	if (!template) {
		errno = EFAULT;
		return NULL;
	}

	len = strlen(template);
	tmp_template = PSEUDO_ROOT_PATH(AT_FDCWD, template, AT_SYMLINK_NOFOLLOW);

	if (!tmp_template) {
		errno = ENOENT;
		return NULL;
	}

	rc = real_mkdtemp(tmp_template);

	if (rc != NULL) {
		save_errno = errno;

		if (base_stat(rc, &buf) != -1) {
			pseudo_client_op(OP_CREAT, 0, -1, -1, tmp_template, &buf);
		} else {
			pseudo_debug(PDBGF_CONSISTENCY, "mkdtemp (path %s) succeeded, but fstat failed (%s).\n",
				rc, strerror(errno));
		}
		errno = save_errno;
	}
	/* mkdtemp only changes the XXXXXX at the end. */
	memcpy(template + len - 6, tmp_template + strlen(tmp_template) - 6, 6);
	rc = template;
	free(tmp_template);
/*	return rc;
 * }
 */
