/*
 * Copyright (c) 2011 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * int execlp(const char *file, const char *arg, va_list ap)
 *	int rc = -1;
 */

	rc = real_execlp(file, arg, ap);

/*	return rc;
 * }
 */
