/*
 * Copyright (c) 2013 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * int msync(void *addr, size_t length, int flags)
 *	int rc = -1;
 */

	/* note: wrapper will never call this if PSEUDO_FORCE_ASYNC
	 * is defined.
	 */
	rc = real_msync(addr, length, flags);

/*	return rc;
 * }
 */
