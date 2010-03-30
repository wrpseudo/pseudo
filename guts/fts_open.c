/* 
 * Copyright (c) 2010 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * static FTS *
 * wrap_fts_open(char * const *path_argv, int options, int (*compar)(const FTSENT **, const FTSENT **)) {
 *	FTS * rc = NULL;
 */
 	char **rpath_argv;
	int args = 0;
	int errored = 0;
	int i;

	if (!path_argv) {
		errno = EFAULT;
		return NULL;
	}
	/* count args */
	for (i = 0; path_argv[i]; ++i) {
		++args;
	}
	rpath_argv = malloc((args + 1) * sizeof(*rpath_argv));
	if (!rpath_argv) {
		errno = ENOMEM;
		return NULL;
	}

	for (i = 0; i < args; ++i) {
		rpath_argv[i] = PSEUDO_ROOT_PATH(AT_FDCWD, path_argv[i], AT_SYMLINK_NOFOLLOW);
		if (!rpath_argv[i])
			errored = 1;
	}

	if (errored) {
		errno = ENOMEM;
		rc = NULL;
	} else {
		rc = real_fts_open(path_argv, options, compar);
	}
	for (i = 0; i < args; ++i)
		free(rpath_argv[i]);
	free(rpath_argv);

/*	return rc;
 * }
 */
