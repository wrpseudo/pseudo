@name pseudo_tables.h
@header
/* standard ranges/values/keys */

/* This file is generated and should not be modified.  See the maketables
 * script if you want to modify this. */

/* NULL, strcmp */
#include <string.h>
@body
/* tables for ${name} */
${comment}
typedef enum {
	${prefix}_UNKNOWN = -1,
	${prefix}_NONE = 0,
	${enums},
	${prefix}_MAX
} pseudo_${name}_t;
${flag_enums}
extern const char *pseudo_${name}_name(pseudo_${name}_t);
extern pseudo_${name}_t pseudo_${name}_id(const char *);
${column_protos}

@footer
