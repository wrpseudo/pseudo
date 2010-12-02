@name pseudo_wrapfuncs.c
@header
/* wrapper functions. generated automatically. */

/* IF YOU ARE SEEING COMPILER ERRORS IN THIS FILE:
 * If you are seeing a whole lot of errors, make sure you aren't actually
 * trying to compile pseudo_wrapfuncs.c directly.  This file is #included
 * from pseudo_wrappers.c, which has a lot of needed include files and
 * static declarations.
 */

/* This file is generated and should not be modified.  See the makewrappers
 * script if you want to modify this. */
@body

static ${type} (*real_${name})(${decl_args}) = NULL;

${type}
${name}(${decl_args}) {
	sigset_t saved;
	${variadic_decl}
	${rc_decl}

	${variadic_start}

	pseudo_debug(4, "called: ${name}\n");
	pseudo_sigblock(&saved);
	if (pseudo_getlock()) {
		errno = EBUSY;
		sigprocmask(SIG_SETMASK, &saved, NULL);
		${def_return}
	}
	if (pseudo_populate_wrappers()) {
		int save_errno;
		if (antimagic > 0) {
			if (real_$name) {
				/* call the real syscall */
				${rc_assign} (*real_${name})(${call_args});
			} else {
				/* rc was initialized to the "failure" value */
				pseudo_enosys("${name}");
			}
		} else {
			${alloc_paths}
			/* exec*() use this to restore the sig mask */
			pseudo_saved_sigmask = saved;
			${rc_assign} wrap_$name(${call_args});
			${free_paths}
		}
		${variadic_end}
		save_errno = errno;
		pseudo_droplock();
		sigprocmask(SIG_SETMASK, &saved, NULL);
		pseudo_debug(4, "completed: $name\n");
		errno = save_errno;
		${rc_return}
	} else {
		pseudo_droplock();
		sigprocmask(SIG_SETMASK, &saved, NULL);
		pseudo_debug(4, "completed: $name\n");
		/* rc was initialized to the "failure" value */
		pseudo_enosys("${name}");
		${variadic_end}
		${rc_return}
	}
}

static ${type}
wrap_${name}(${wrap_args}) {
	$rc_decl
	${maybe_variadic_decl}
	${maybe_variadic_start}

#include "guts/${name}.c"

	${rc_return}
}

