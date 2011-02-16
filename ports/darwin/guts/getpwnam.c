/*
 * Copyright (c) 2011 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * struct passwd *getpwnam(const char *name)
 *	struct passwd *rc = NULL;
 */

	rc = real_getpwnam(name);

/*	return rc;
 * }
 */
