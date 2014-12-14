/*
 * Copyright (c) 2014 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * mode_t umask(mode_t mask)
 *	mode_t rc = 0;
 */

	pseudo_umask = mask;
	rc = real_umask(mask);

/*	return rc;
 * }
 */
