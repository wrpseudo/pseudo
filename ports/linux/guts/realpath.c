/* 
 * Copyright (c) 2010 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * static char *
 * wrap_realpath(const char *name, char *resolved_name) {
 *	char * rc = NULL;
 */
	char *rname = PSEUDO_ROOT_PATH(AT_FDCWD, name, 0);
	ssize_t len;
	if (!rname) {
		errno = ENAMETOOLONG;
		return NULL;
	}
	if ((len = strlen(rname)) >= pseudo_sys_path_max()) {
		free(rname);
		errno = ENAMETOOLONG;
		return NULL;
	}
	if (resolved_name) {
		memcpy(resolved_name, rname, len + 1);
		free(rname);
		rc = resolved_name;
	} else {
		rc = rname;
	}

/*	return rc;
 * }
 */
