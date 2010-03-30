/* 
 * Copyright (c) 2008-2010 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * static int
 * wrap___lxstat64(int ver, const char *path, struct stat64 *buf) {
 *	int rc = -1;
 */

	rc = wrap___fxstatat64(ver, AT_FDCWD, path, buf, AT_SYMLINK_NOFOLLOW);

/*
 * }
 */
