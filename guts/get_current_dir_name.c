/* 
 * static char *
 * wrap_get_current_dir_name(void) {
 *	char * rc = NULL;
 */

	pseudo_debug(3, "get_current_dir_name (getcwd)\n");
	/* this relies on a Linux extension, but we dutifully
	 * emulated that extension. 
	 */
	rc = wrap_getcwd(NULL, 0);

/*	return rc;
 * }
 */
