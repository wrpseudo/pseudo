/* 
 * Copyright (c) 2008-2011 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * static int
 * clone(...) {
 *      ....
 */
	/* because clone() doesn't actually continue in this function, we
	 * can't check the return and fix up environment variables in the
	 * child.  Instead, we have to create an intermediate function,
	 * wrap_clone_child, which does this fix up.
	 */

	struct clone_args * myargs = malloc(sizeof(struct clone_args));

	myargs->fn = fn;
	myargs->flags = flags;
	myargs->arg = arg;

	/* call the real syscall */
	rc = (*real_clone)(wrap_clone_child, child_stack, flags, myargs, pid);

	/* If we're not sharing memory, we need to free myargs in the parent */
	if (!(flags & CLONE_VM))
		free(myargs);

/*	...
 *	return rc;
 * }
 */		
