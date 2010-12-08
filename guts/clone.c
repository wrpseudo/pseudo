/* 
 * Copyright (c) 2008-2010 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * static int
 * clone(...) {
 *      ....
 */
	/* because clone() doesn't actually continue in this function, we
	 * can't check the return and fix up environment variables in the
	 * child.  Instead, we have to temporarily do any fixup, then possibly
	 * undo it later.  UGH!
	 */
	pseudo_debug(1, "client resetting for clone(2) call\n");
	if (real_clone) {
		if (!pseudo_get_value("PSEUDO_RELOADED")) {
			pseudo_setupenv();
			pseudo_reinit_libpseudo();
		} else {
			pseudo_setupenv();
			pseudo_dropenv();
		}
		/* call the real syscall */
		rc = (*real_clone)(fn, child_stack, flags, arg, pid, tls, ctid);
	} else {
		/* rc was initialized to the "failure" value */
		pseudo_enosys("clone");
	}

/*	...
 *	return rc;
 * }
 */		
