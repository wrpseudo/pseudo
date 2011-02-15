/*
 * Copyright (c) 2011 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * int dup2(int oldfd, int newfd)
 *	int rc = -1;
 */

	rc = real_dup2(oldfd, newfd);

/*	return rc;
 * }
 */
