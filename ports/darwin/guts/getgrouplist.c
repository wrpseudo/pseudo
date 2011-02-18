/* 
 * Copyright (c) 2010 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * static int
 * wrap_getgrouplist(const char *name, int basegid, int *groups, int *ngroups) {
 *	int rc = -1;
 */

	int found = 0;
	int found_group = 0;
	char buf[PSEUDO_PWD_MAX];
	struct group grp, *gbuf = &grp;

	setgrent();
	while ((rc = wrap_getgrent_r(gbuf, buf, PSEUDO_PWD_MAX, &gbuf)) == 0) {
		int i = 0;
		for (i = 0; gbuf->gr_mem[i]; ++i) {
			if (!strcmp(gbuf->gr_mem[i], name)) {
				if (found < *ngroups)
					groups[found] = gbuf->gr_gid;
				++found;
				if ((int) gbuf->gr_gid == basegid)
					found_group = 1;
			}
		}
	}
	endgrent();
	if (!found_group) {
		if (found < *ngroups)
			groups[found] = basegid;
		++found;
	}
	if (found >= *ngroups)
		rc = -1;
	else
		rc = found;
	*ngroups = found;

/*	return rc;
 * }
 */
