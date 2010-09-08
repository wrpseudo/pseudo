/* 
 * Copyright (c) 2008-2010 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * static int
 * wrap_mkfifoat(int dirfd, const char *path, mode_t mode) {
 *	int rc = -1;
 */
 	dev_t unused = 0;

	rc = wrap___xmknodat(_STAT_VER, dirfd, path, (mode & 07777) | S_IFIFO, &unused);

/*	return rc;
 * }
 */
