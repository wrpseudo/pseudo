/* 
 * Copyright (c) 2008-2010 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * static int
 * wrap___fxstatat(int ver, int dirfd, const char *path, struct stat *buf, int flags) {
 *	int rc = -1;
 */

	struct stat64 buf64;
	/* populate buffer with complete data */
#ifdef PSEUDO_NO_REAL_AT_FUNCTIONS
	if (dirfd != AT_FDCWD) {
		errno = ENOSYS;
		return -1;
	}
	if (flags & AT_SYMLINK_NOFOLLOW) {
		rc = real___lxstat(ver, path, buf);
	} else {
		rc = real___xstat(ver, path, buf);
	}
#else
	real___fxstatat(ver, dirfd, path, buf, flags);
#endif
	/* obtain fake data */
	rc = wrap___fxstatat64(ver, dirfd, path, &buf64, flags);
	/* overwrite */
	pseudo_stat32_from64(buf, &buf64);

/*	return rc;
 * }
 */
