/* 
 * Copyright (c) 2010 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * static int
 * wrap_execv(const char *path, char *const *argv) {
 *	int rc = -1;
 */

	if (!pseudo_get_value("PSEUDO_RELOADED"))
		pseudo_setupenv();
	else {
		pseudo_setupenv();
		pseudo_dropenv();
	}

	rc = real_execv(path, argv);

/*	return rc;
 * }
 */
