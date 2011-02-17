@name pseudo_wrapfuncs.h
@header
/* wrapper functions. generated automatically. */

/* This file is generated and should not be modified.  See the makewrappers
 * script if you want to modify this. */
@body
/* ${comment} */
static ${type} wrap_${name}(${wrap_args});
static ${type} (*real_${name})(${decl_args});
${real_predecl}
@footer
