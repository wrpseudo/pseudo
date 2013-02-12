/* 
 * Copyright (c) 2008-2010, 2012, 2013 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * static int
 * wrap_link(const char *oldname, const char *newname) {
 *	int rc = -1;
 */
        /* since 2.6.18 or so, linkat supports AT_SYMLINK_FOLLOW, which
         * provides the behavior link() has on most non-Linux systems,
         * but the default is not to follow symlinks. Better yet, it
         * does NOT support AT_SYMLINK_NOFOLLOW! So define this in
         * your port's portdefs.h or hope the default works for you.
         */
        rc = wrap_linkat(AT_FDCWD, oldname, AT_FDCWD, newname,
                         PSEUDO_LINK_SYMLINK_BEHAVIOR);

/*	return rc;
 * }
 */
