/* 
 * Copyright (c) 2008-2010 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * static int
 * wrap_execvp(const char *file, char *const *argv) {
 *	int rc = -1;
 */
	environ = pseudo_setupenv(environ, getenv("PSEUDO_OPTS"));

	rc = real_execvp(file, argv);

/*	return rc;
 * }
 */
