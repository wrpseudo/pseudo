/* 
 * Copyright (c) 2010 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * static int
 * wrap_getgroups(int size, gid_t *list) {
 *	int rc = -1;
 */
	struct passwd *p = wrap_getpwuid(wrap_getuid());
	int oldsize = size;

	if (p) {
		rc = wrap_getgrouplist(p->pw_name, wrap_getgid(), (int *) list, &size);
		if (oldsize == 0 || size <= oldsize)
			rc = size;
	} else {
		errno = ENOENT;
	}

/*	return rc;
 * }
 */
