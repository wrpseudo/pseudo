/*
 * Copyright (c) 2011 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * int execle(const char *file, const char *arg, va_list ap)
 *	int rc = -1;
 */

	rc = real_execle(file, arg, ap);

/*	return rc;
 * }
 */
