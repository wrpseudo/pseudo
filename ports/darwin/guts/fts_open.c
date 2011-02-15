/*
 * Copyright (c) 2011 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * FTS *fts_open(char * const *path_argv, int options, int (*compar)(const FTSENT **, const FTSENT **))
 *	FTS *rc = NULL;
 */

	rc = real_fts_open(path_argv, options, compar);

/*	return rc;
 * }
 */
