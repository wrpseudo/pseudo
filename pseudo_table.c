/*
 * pseudo_table.c, data definitions and related utilities
 *
 * Copyright (c) 2008-2010 Wind River Systems, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the Lesser GNU General Public License version 2.1 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the Lesser GNU General Public License for more details.
 *
 * You should have received a copy of the Lesser GNU General Public License
 * version 2.1 along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA 
 *
 */
#include "pseudo.h"
#include <stdlib.h>
#include <string.h>

/* just a bunch of handy lookups for pretty-printing stuff */

static char *operation_names[] = {
	"none",
	"chdir",
	"chmod",
	"chown",
	"chroot",
	"close",
	"creat",
	"dup",
	"fchmod",
	"fchown",
	"fstat",
	"link",
	"mkdir",
	"mknod",
	"open",
	"rename",
	"stat",
	"unlink",
	"symlink",
	"exec",
	NULL
};

static char *result_names[] = {
	"none",
	"succeed",
	"fail",
	"error",
	NULL
};

static char *severity_names[] = {
	"none",
	"debug",
	"info",
	"warn",
	"error",
	"critical",
	NULL
};

static char *query_type_names[] = {
	"none",
	"exact",
	"less",
	"greater",
	"bitand",
	"notequal",
	"like",
	"notlike",
	"sqlpat",
	NULL
};

static char *query_type_sql[] = {
	"LITTLE BOBBY TABLES",
	"=",
	"<",
	">",
	"&",
	"!=",
	"LIKE",
	"NOT LIKE",
	"LIKE",
	NULL
};

static char *query_field_names[] = {
	"zero",
	"access",
	"client",
	"dev",
	"fd",
	"ftype",
	"gid",
	"id",
	"ino",
	"mode",
	"op",
	"order",
	"path",
	"perm",
	"result",
	"severity",
	"stamp",
	"tag",
	"text",
	"uid",
	NULL
};

char *
pseudo_op_name(op_id_t id) {
	if (id >= OP_NONE && id < OP_MAX) {
		return operation_names[id];
	}
	return "unknown";
}

char *
pseudo_res_name(res_id_t id) {
	if (id >= RESULT_NONE && id < RESULT_MAX) {
		return result_names[id];
	}
	return "unknown";
}

char *
pseudo_sev_name(sev_id_t id) {
	if (id >= SEVERITY_NONE && id < SEVERITY_MAX) {
		return severity_names[id];
	}
	return "unknown";
}

char *
pseudo_query_type_name(pseudo_query_type_t id) {
	if (id >= PSQT_NONE && id < PSQT_MAX) {
		return query_type_names[id];
	}
	return "unknown";
}

char *
pseudo_query_type_sql(pseudo_query_type_t id) {
	if (id >= PSQT_NONE && id < PSQT_MAX) {
		return query_type_sql[id];
	}
	return "LITTLE BOBBY TABLES";
}

char *
pseudo_query_field_name(pseudo_query_field_t id) {
	if (id >= PSQF_NONE && id < PSQF_MAX) {
		return query_field_names[id];
	}
	return "unknown";
}

op_id_t
pseudo_op_id(char *name) {
	int id;
	for (id = OP_NONE; id < OP_MAX; ++id) {
		if (!strcmp(name, operation_names[id]))
			return id;
	}
	return OP_UNKNOWN;
}

res_id_t
pseudo_res_id(char *name) {
	int id;
	for (id = RESULT_NONE; id < RESULT_MAX; ++id) {
		if (!strcmp(name, result_names[id]))
			return id;
	}
	return RESULT_UNKNOWN;
}

sev_id_t
pseudo_sev_id(char *name) {
	int id;
	for (id = SEVERITY_NONE; id < SEVERITY_MAX; ++id) {
		if (!strcmp(name, severity_names[id]))
			return id;
	}
	return SEVERITY_UNKNOWN;
}

pseudo_query_type_t
pseudo_query_type(char *name) {
	int id;
	for (id = PSQT_NONE; id < PSQT_MAX; ++id) {
		if (!strcmp(name, query_type_names[id]))
			return id;
	}
	return PSQT_UNKNOWN;
}

pseudo_query_field_t
pseudo_query_field(char *name) {
	int id;
	for (id = PSQF_NONE; id < PSQF_MAX; ++id) {
		if (!strcmp(name, query_field_names[id]))
			return id;
	}
	return PSQF_UNKNOWN;
}
