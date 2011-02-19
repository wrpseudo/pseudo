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
/* int execv(const char *file, char *const *argv) */
static int wrap_execv(const char *file, char *const *argv);
static int (*real_execv)(const char *file, char *const *argv);
/* int execve(const char *file, char *const *argv, char *const *envp) */
static int wrap_execve(const char *file, char *const *argv, char *const *envp);
static int (*real_execve)(const char *file, char *const *argv, char *const *envp);
/* int execvp(const char *file, char *const *argv) */
static int wrap_execvp(const char *file, char *const *argv);
static int (*real_execvp)(const char *file, char *const *argv);

