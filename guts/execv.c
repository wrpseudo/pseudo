/* 
 * Copyright (c) 2010 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * static int
 * wrap_execv(const char *path, char *const *argv) {
 *	int rc = -1;
 */
	environ = pseudo_setupenv(environ, getenv("PSEUDO_OPTS"));

	rc = real_execv(path, argv);

/*	return rc;
 * }
 */
