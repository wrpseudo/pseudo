/*
 * Copyright (c) 2011 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * struct passwd *getpwuid(uid_t uid)
 *	struct passwd *rc = NULL;
 */

	rc = real_getpwuid(uid);

/*	return rc;
 * }
 */
