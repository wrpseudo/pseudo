/*
 * Copyright (c) 2011 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * struct group *getgrnam(const char *name)
 *	struct group *rc = NULL;
 */

	rc = real_getgrnam(name);

/*	return rc;
 * }
 */
