/* 
 * Copyright (c) 2008-2010 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * static char *
 * wrap_get_current_dir_name(void) {
 *	char * rc = NULL;
 */

	/* this relies on a Linux extension, but we dutifully
	 * emulated that extension. 
	 */
	rc = wrap_getcwd(NULL, 0);

/*	return rc;
 * }
 */
