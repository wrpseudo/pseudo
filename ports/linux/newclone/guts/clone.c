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
	pseudo_setupenv();
	if (!pseudo_get_value("PSEUDO_UNLOAD")) {
		pseudo_reinit_libpseudo();
	} else {
		pseudo_dropenv();
	}
	/* call the real syscall */
	rc = (*real_clone)(fn, child_stack, flags, arg, pid, tls, ctid);
/*	...
 *	return rc;
 * }
 */		
