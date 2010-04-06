/* 
 * Copyright (c) 2010 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * static char *
 * wrap_mktemp(char *template) {
 *	char * rc = NULL;
 */
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

	rc = real_mktemp(tmp_template);

	/* mktemp only changes the XXXXXX at the end, and never created
	 * a file -- note the race condition implied here.
	 */
	memcpy(template + len - 6, tmp_template + strlen(tmp_template) - 6, 6);
	rc = template;
	free(tmp_template);

/*	return rc;
 * }
 */
