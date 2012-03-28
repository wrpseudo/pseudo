/*
 * Copyright (c) 2011 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * int execl(const char *file, const char *arg, va_list ap)
 *	int rc = -1;
 */

	/* NOTE THAT THIS IS NEVER USED!
	 * We implement all execl() in terms of execv()
	 * so this call is not used.
	 */

	rc = real_execl(file, arg, ap);

/*	return rc;
 * }
 */
