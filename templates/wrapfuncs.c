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

static ${type} (*real_${name})(${decl_args}) = ${real_init};

${maybe_skip}

${type}
${name}(${decl_args}) {
	sigset_t saved;
	${variadic_decl}
	${rc_decl}

${maybe_async_skip}

	if (!pseudo_check_wrappers() || !real_$name) {
		/* rc was initialized to the "failure" value */
		pseudo_enosys("${name}");
		${rc_return}
	}

	${variadic_start}

	if (pseudo_disabled) {
		${rc_assign} (*real_${name})(${call_args});
		${variadic_end}
		${rc_return}
	}

	pseudo_debug(PDBGF_WRAPPER, "wrapper called: ${name}\n");
	pseudo_sigblock(&saved);
	pseudo_debug(PDBGF_WRAPPER | PDBGF_VERBOSE, "${name} - signals blocked, obtaining lock\n");
	if (pseudo_getlock()) {
		errno = EBUSY;
		sigprocmask(SIG_SETMASK, &saved, NULL);
		pseudo_debug(PDBGF_WRAPPER, "${name} failed to get lock, giving EBUSY.\n");
		${def_return}
	}

	int save_errno;
	if (antimagic > 0) {
		/* call the real syscall */
		pseudo_debug(PDBGF_SYSCALL, "${name} calling real syscall.\n");
		${rc_assign} (*real_${name})(${call_args});
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
	pseudo_debug(PDBGF_WRAPPER | PDBGF_VERBOSE, "${name} - yielded lock, restored signals\n");
#if 0
/* This can cause hangs on some recentish systems which use locale
 * stuff for strerror...
 */
	pseudo_debug(PDBGF_WRAPPER, "wrapper completed: ${name} (maybe: %s)\n", strerror(save_errno));
#endif
	pseudo_debug(PDBGF_WRAPPER, "wrapper completed: ${name} (errno: %d)\n", save_errno);
	errno = save_errno;
	${rc_return}
}

static ${type}
wrap_${name}(${wrap_args}) {
	$rc_decl
	${maybe_variadic_decl}
	${maybe_variadic_start}

#include "ports/${port}/guts/${name}.c"

	${rc_return}
}

${end_maybe_skip}
