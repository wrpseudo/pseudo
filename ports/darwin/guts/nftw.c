/*
 * Copyright (c) 2011 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * int nftw(const char *path, int (*fn)(const char *, const struct stat *, int, struct FTW *), int nopenfd, int flag)
 *	int rc = -1;
 */

	rc = real_nftw(path, fn, nopenfd, flag);

/*	return rc;
 * }
 */
