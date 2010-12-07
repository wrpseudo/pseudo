@name pseudo_wrapfuncs.h
@header
/* wrapper functions. generated automatically. */

/* This file is generated and should not be modified.  See the makewrappers
 * script if you want to modify this. */
@body
/* ${comment} */
static ${type} wrap_${name}(${wrap_args});
static ${type} (*real_${name})(${decl_args});
@footer
/* special cases: functions with manually-written wrappers */

/* int fork(void) */
static int wrap_fork(void);
static int (*real_fork)(void);
static int wrap_clone(int (*fn)(void *), void *child_stack, int flags, void *arg, ...);
static int (*real_clone)(int (*fn)(void *), void *child_stack, int flags, void *arg, ...);
