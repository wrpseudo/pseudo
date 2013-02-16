/*
 * Copyright (c) 2013 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * void sync(void)
 *	
 */

	/* note: wrapper will never call this if PSEUDO_FORCE_ASYNC
	 * is defined.
	 */
	(void) real_sync();

/*	return;
 * }
 */
