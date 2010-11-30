@name pseudo_tables.c
@header
/* Tables matching enums to strings */

/* This file is generated and should not be modified.  See the maketables
 * script if you want to modify this. */

#include "pseudo_tables.h"

@body
/* tables for ${name} */

static const char *${name}_id_to_name[] = {
	"none",
	${names},
	NULL
};
${column_names}

/* functions for ${name} */
extern const char *
pseudo_${name}_name(pseudo_${name}_t id) {
	if (id < 0 || id >= ${prefix}_MAX)
		return "unknown";
	return ${name}_id_to_name[id];
}

extern pseudo_${name}_t
pseudo_${name}_id(const char *name) {
	int id;

	if (!name)
		return -1;

	for (id = 0; id < ${prefix}_MAX; ++id)
		if (!strcmp(${name}_id_to_name[id], name))
			return id;

	return -1;
}
${column_funcs}

@footer
