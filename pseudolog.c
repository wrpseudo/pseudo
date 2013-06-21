/*
 * pseudolog.c, pseudo database viewer (preliminary)
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
/* We need _XOPEN_SOURCE for strptime(), but if we define that,
 * we then don't get S_IFSOCK... _GNU_SOURCE turns on everything. */
#define _GNU_SOURCE

#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>

#include "pseudo.h"
#include "pseudo_ipc.h"
#include "pseudo_client.h"
#include "pseudo_db.h"

static int opt_D = 0;
static int opt_U = 0;
static int opt_l = 0;

/* when the client is linked with pseudo_wrappers, these are defined there.
 * when it is linked with pseudo_server, though, we have to provide different
 * versions (pseudo_wrappers must not be linked with the server, or Bad Things
 * happen).
 */
void pseudo_magic(void) { }
void pseudo_antimagic(void) { }

static void display(log_entry *, char *format);
static int format_scan(char *format);

void
usage(int status) {
	static char *options[] = {
		"a  access (rwax)",
		"c  client pid",
		"d  device number",
		"f  file descriptor",
		"g  gid",
		"G  tag (text)",
		"i  inode number",
		"I  id (database row)",
		"m  permission bits (octal)",
		"M  file mode (octal)",
		"o  operation (e.g. 'open')",
		"O  order by (< DESC > ASC)",
		"p  file path",
		"P  program",
		"r  result (e.g. 'succeed')",
		"s  timestamp",
		"S  severity",
		"t  type (like find -type)",
		"T  text (text field)",
		"u  uid",
		"y  type (op/ping/shutown)",
		NULL,
	};
	FILE *f = (status == EXIT_SUCCESS) ? stdout : stderr;
	int i;

	fputs("pseudolog: create or report log entries. usage:\n", f);
	fputs("pseudolog -l [SPECIFIERS] -- create entries\n", f);
	fputs("pseudolog [-U] [-F format] [SPECIFIERS] -- report entries\n", f);
	fputs("pseudolog -D [SPECIFIERS] -- delete entries\n", f);
	fputs("shared options: [-P prefix] [-E timeformat] [-x flags]\n", f);
	fputs("  format is a printf-like format string using the option letters\n", f);
	fputs("  listed below as format specifiers for the corresponding field.\n", f);
	fputs("  timeformat is a strftime-like format string, the default is '%x %X'.\n", f);
	fputs("  prefix is the PSEUDO_PREFIX in which to find the database.\n", f);
	fputs("  flags are characters from the same debug flag set used by pseudo.\n", f);
	fputs("\n", f);
	fputs("SPECIFIERS are options of the form -X <value>, where X is one of\n", f);
	fputs("the following option letters, and value is the value to match.\n", f);
	fputs("values may be prefixed with ! (not equal to), > (greater than),\n", f);
	fputs("< (less than), & (bitwise and), ~ (LIKE match, anchored at both\n", f);
	fputs("ends, text fields only), ^ (NOT LIKE match, anchored at both\n", f);
	fputs("ends, or % (LIKE match, text fields only).\n", f);
	fputs("\n", f);
	fputs("OPTION LETTERS:\n", f);
	for (i = 0; options[i]; ++i) {
		fprintf(f, "  %-28s%s", options[i], (i % 2) ? "\n" : "   ");
	}
	if (i % 2 == 1) {
		fprintf(f, "\n");
	}
	exit(status);
}

pseudo_query_field_t opt_to_field[UCHAR_MAX + 1] = {
	['a'] = PSQF_ACCESS,
	['c'] = PSQF_CLIENT,
	['d'] = PSQF_DEV,
	['f'] = PSQF_FD,
	['g'] = PSQF_GID,
	['G'] = PSQF_TAG,
	['I'] = PSQF_ID,
	['i'] = PSQF_INODE,
	['m'] = PSQF_PERM,
	['M'] = PSQF_MODE,
	['o'] = PSQF_OP,
	['O'] = PSQF_ORDER,
	['p'] = PSQF_PATH,
	['r'] = PSQF_RESULT,
	['R'] = PSQF_PROGRAM,
	['s'] = PSQF_STAMP,
	['S'] = PSQF_SEVERITY,
	['t'] = PSQF_FTYPE,
	['T'] = PSQF_TEXT,
	['u'] = PSQF_UID,
	['y'] = PSQF_TYPE,
};

pseudo_query_type_t
plog_query_type(char **string) {
	pseudo_query_type_t type = PSQT_EXACT;
	if (!string || !*string)
		return PSQT_UNKNOWN;
	switch (**string) {
	case '\0':
		pseudo_diag("Error: Value may not be an empty string.");
		return PSQT_UNKNOWN;
		break;
	case '>':
		type = PSQT_GREATER;
		++*string;
		break;
	case '<':
		type = PSQT_LESS;
		++*string;
		break;
	case '!':
		type = PSQT_NOTEQUAL;
		++*string;
		break;
	case '=':
		++*string;
		break;
	case '&':
		type = PSQT_BITAND;
		++*string;
		break;
	case '%':
		type = PSQT_LIKE;
		++*string;
		break;
	case '^':
		type = PSQT_NOTLIKE;
		++*string;
		break;
	case '~':
		type = PSQT_SQLPAT;
		++*string;
		break;
	case '\\':
		/* no special type, but allows one of the others to be the
		 * first character of the effective string
		 */
		++*string;
		break;
	}
	if (opt_l && type != PSQT_EXACT) {
		pseudo_diag("Error: Non-exact match requested while trying to create a log entry.\n");
		type = PSQT_UNKNOWN;
	}
	return type;
}

static char *time_formats[] = {
	"%s",
	"%F %r",
	"%F %T",
	"%m-%d %r",
	"%m-%d %T",
	"%r",
	"%T",
	NULL,
};
static char *timeformat = "%X";

mode_t
parse_file_type(char *string) {
	switch (*string) {
	case 'b':
		return S_IFBLK;
		break;
	case 'c':
		return S_IFCHR;
		break;
	case 'd':
		return S_IFDIR;
		break;
	case '-':	/* FALLTHROUGH */
	case 'f':
		return S_IFREG;
		break;
	case 'l':
		return S_IFLNK;
		break;
	case 'p':
		return S_IFIFO;
		break;
	case 's':
		return S_IFSOCK;
		break;
	default:
		pseudo_diag("unknown file type %c; should be one of [-bcdflps]\n",
			isprint(*string) ? *string : '?');
		return -1;
		break;
	}
}

mode_t
parse_partial_mode(char *string) {
	mode_t mode = 0;
	switch (string[0]) {
	case 'r':
		mode |= 04;
		break;
	case '-':
		break;
	default:
		pseudo_diag("unknown mode character: %c\n", string[0]);
		return -1;
		break;
	}
	switch (string[1]) {
	case 'w':
		mode |= 02;
		break;
	case '-':
		break;
	default:
		pseudo_diag("unknown mode character: %c\n", string[1]);
		return -1;
		break;
	}
	switch (string[2]) {
	case 'x':
		mode |= 01;
		break;
	case 't':	/* FALLTHROUGH */
	case 's':
		mode |= 011;
		break;
	case 'T':	/* FALLTHROUGH */
	case 'S':
		mode |= 010;
		break;
	case '-':
		break;
	default:
		pseudo_diag("unknown mode character: %c\n", string[2]);
		return -1;
		break;
	}
	return mode;
}

mode_t
parse_mode_string(char *string) {
	size_t len = strlen(string);
	mode_t mode = 0;
	mode_t bits = 0;

	if (len != 9 && len != 10) {
		pseudo_diag("mode strings must be of the form [-]rwxr-xr-x\n");
		return -1;
	}
	if (len == 10) {
		mode |= parse_file_type(string);
		++string;
		if (mode == (mode_t) -1) {
			pseudo_diag("mode strings with a file type must use a valid type [-bcdflps]\n");
			return -1;
		}
	}
	bits = parse_partial_mode(string);
	if (bits == (mode_t) -1)
		return -1;
	if (bits & 010) {
		mode |= S_ISUID;
		bits &= ~010;
	}
	mode |= bits << 6;
	string += 3;
	bits = parse_partial_mode(string);
	if (bits == (mode_t) -1)
		return -1;
	if (bits & 010) {
		mode |= S_ISGID;
		bits &= ~010;
	}
	mode |= bits << 3;
	string += 3;
	bits = parse_partial_mode(string);
	if (bits == (mode_t) -1)
		return -1;
	if (bits & 010) {
		mode |= S_ISVTX;
		bits &= ~010;
	}
	mode |= bits;
	return mode;
}

static time_t
parse_timestamp(char *string) {
	time_t stamp_sec;
	struct tm stamp_tm;
	int i;
	char *s;
	char timebuf[4096];

	stamp_sec = time(0);

	/* try the user's provided time format first, if there is one: */
	localtime_r(&stamp_sec, &stamp_tm);
	s = strptime(string, timeformat, &stamp_tm);
	if (s && !*s) {
		return mktime(&stamp_tm);
	}

	for (i = 0; time_formats[i]; ++i) {
		char *s;
		localtime_r(&stamp_sec, &stamp_tm);
		s = strptime(string, time_formats[i], &stamp_tm);
		if (s && !*s) {
			break;
		}
	}
	if (!time_formats[i]) {
		pseudo_diag("Couldn't parse <%s> as a time.  Current time in known formats is:\n",
			string);
		localtime_r(&stamp_sec, &stamp_tm);
		for (i = 0; time_formats[i]; ++i) {
			strftime(timebuf, sizeof(timebuf), time_formats[i], &stamp_tm);
			pseudo_diag("\t%s\n", timebuf);
		}
		pseudo_diag("Or, specify your own with -E; see strptime(3).\n");
		return -1;
	}
	return mktime(&stamp_tm);
}

pseudo_query_t *
plog_trait(int opt, char *string) {
	pseudo_query_t *new_trait;
	char *endptr;

	if (opt < 0 || opt > UCHAR_MAX) {
		pseudo_diag("Unknown/invalid option value: %d\n", opt);
		return 0;
	}
	if (!opt_to_field[opt]) {
		if (isprint(opt)) {
			pseudo_diag("Unknown option: -%c\n", opt);
		} else {
			pseudo_diag("Unknown option: 0x%02x\n", opt);
		}
		return 0;
	}
	if (!*string) {
		pseudo_diag("invalid empty string for -%c\n", opt);
		return 0;
	}
	new_trait = calloc(sizeof(*new_trait), 1);
	if (!new_trait) {
		pseudo_diag("Couldn't allocate requested trait (for -%c %s)\n",
			opt, string ? string : "<nil>");
		return 0;
	}
	new_trait->field = opt_to_field[opt];
	new_trait->type = plog_query_type(&string);
	if (new_trait->type == PSQT_UNKNOWN) {
		pseudo_diag("Couldn't comprehend trait type for '%s'\n",
			string ? string : "<nil>");
		free(new_trait);
		return 0;
	}
	switch (new_trait->field) {
	case PSQF_ACCESS:
		new_trait->data.ivalue = pseudo_access_fopen(string);
		if (new_trait->data.ivalue == (unsigned long long) -1) {
			pseudo_diag("access flags should be specified like fopen(3) mode strings (or x for exec).\n");
			free(new_trait);
			return 0;
		}
		break;
	case PSQF_FTYPE:
		/* special magic: allow file types ala find */
		/* This is implemented by additional magic over in the database code */
		/* must not be more than one character.  The test against
		 * the first character is because in theory, if the
		 * first character is the terminating NUL, we may not
		 * access the second. */
		if (string[0] && string[1]) {
			pseudo_diag("file type must be a single character [-bcdflps].\n");
			free(new_trait);
			return 0;
		}
		new_trait->data.ivalue = parse_file_type(string);
		if (new_trait->data.ivalue == (mode_t) -1) {
			free(new_trait);
			return 0;
		}
		break;
	case PSQF_OP:
		new_trait->data.ivalue = pseudo_op_id(string);
		break;
	case PSQF_ORDER:
		if (string[0] && string[1]) {
			pseudo_diag("order type must be a single specifier character.\n");
			free(new_trait);
			return 0;
		}
		new_trait->data.ivalue = opt_to_field[(unsigned char) string[0]];
		if (!new_trait->data.ivalue) {
			pseudo_diag("Unknown field type: %c\n", string[0]);
		}
		break;
	case PSQF_RESULT:
		new_trait->data.ivalue = pseudo_res_id(string);
		break;
	case PSQF_SEVERITY:
		new_trait->data.ivalue = pseudo_sev_id(string);
		break;
	case PSQF_STAMP:
		new_trait->data.ivalue = parse_timestamp(string);
		if ((time_t) new_trait->data.ivalue == (time_t) -1) {
			free(new_trait);
			return 0;
		}
		break;
	case PSQF_TYPE:
		new_trait->data.ivalue = pseudo_msg_type_id(string);
		break;
	case PSQF_CLIENT:
	case PSQF_DEV:
	case PSQF_FD:
	case PSQF_GID:
	case PSQF_INODE:
	case PSQF_UID:
		new_trait->data.ivalue = strtoll(string, &endptr, 0);
		if (*endptr) {
			pseudo_diag("Unexpected garbage after number (%llu): '%s'\n",
				new_trait->data.ivalue, endptr);
			free(new_trait);
			return 0;
		}
		break;
	case PSQF_MODE:
	case PSQF_PERM:
		new_trait->data.ivalue = strtoll(string, &endptr, 8);
		if (!*endptr) {
			break;
		}
		/* maybe it's a mode string? */
		new_trait->data.ivalue = parse_mode_string(string);
		if (new_trait->data.ivalue == (mode_t) -1) {
			free(new_trait);
			return 0;
		}
		if (new_trait->field == PSQF_PERM) {
			/* mask out file type */
			new_trait->data.ivalue &= ~S_IFMT;
		}
		break;
	case PSQF_PATH:		/* FALLTHROUGH */
	case PSQF_PROGRAM:	/* FALLTHROUGH */
	case PSQF_TEXT:		/* FALLTHROUGH */
	case PSQF_TAG:
		/* Plain strings */
		new_trait->data.svalue = strdup(string);
		break;
	default:
		pseudo_diag("I don't know how I got here.  Unknown field type %d.\n",
			new_trait->field);
		free(new_trait);
		return 0;
		break;
	}
	return new_trait;
}

/* You can either create a query or create a log entry.  They use very
 * similar syntax, but:
 * - if you're making a query, you can use >, <, etc.
 * - if you're logging, you can't.
 * This is tracked by recording whether any non-exact relations
 * have been requested ("query_only"), and refusing to set the -l
 * flag if they have, and refusing to accept any such relation
 * if the -l flag is already set.
 */
int
main(int argc, char **argv) {
	pseudo_query_t *traits = 0, *current = 0, *new_trait = 0;
	char *s;
	log_history history;
	int query_only = 0;
	int o;
	int bad_args = 0;
	char *format = "%s %-12.12R %-4y %7o: [mode %04m, %2a] %p %T";

	while ((o = getopt(argc, argv, "vla:c:d:DE:f:F:g:G:hi:I:m:M:o:O:p:P:r:R:s:S:t:T:u:Ux:y:")) != -1) {
		switch (o) {
		case 'P':
			s = PSEUDO_ROOT_PATH(AT_FDCWD, optarg, AT_SYMLINK_NOFOLLOW);
			if (!s)
				pseudo_diag("Can't resolve prefix path '%s'\n", optarg);
			pseudo_set_value("PSEUDO_PREFIX", s);
			break;
		case 'v':
			pseudo_debug_verbose();
			break;
		case 'x':
			pseudo_debug_set(optarg);
			break;
		case 'l':
			opt_l = 1;
			break;
		case 'D':
			opt_D = 1;
			query_only = 1;
			break;
		case 'E':
			timeformat = strdup(optarg);
			break;
		case 'F':
			/* disallow specifying -F with -l */
			format = strdup(optarg);
			query_only = 1;
			break;
		case 'U':
			opt_U = 1;
			query_only = 1;
			break;
		case 'I':		/* PSQF_ID */
			query_only = 1;
					/* FALLTHROUGH */
		case 'a':		/* PSQF_ACCESS */
		case 'c':		/* PSQF_CLIENT */
		case 'd':		/* PSQF_DEV */
		case 'f':		/* PSQF_FD */
		case 'g':		/* PSQF_GID */
		case 'G':		/* PSQF_TAG */
		case 'i':		/* PSQF_INODE */
		case 'm':		/* PSQF_PERM */
		case 'M':		/* PSQF_MODE */
		case 'o':		/* PSQF_OP */
		case 'O':		/* PSQF_ORDER */
		case 'p':		/* PSQF_PATH */
		case 'r':		/* PSQF_RESULT */
		case 'R':		/* PSQF_PROGRAM */
		case 's':		/* PSQF_STAMP */
		case 'S':		/* PSQF_SEVERITY */
		case 't':		/* PSQF_FTYPE */
		case 'T':		/* PSQF_TEXT */
		case 'u':		/* PSQF_UID */
		case 'y':		/* PSQF_TYPE */
			new_trait = plog_trait(o, optarg);
			if (!new_trait) {
				bad_args = 1;
			}
			break;
		case 'h':
			usage(EXIT_SUCCESS);
			break;
		case '?':		/* FALLTHROUGH */
		default:
			fprintf(stderr, "unknown option '%c'\n", optopt);
			usage(EXIT_FAILURE);
			break;
		}
		if (new_trait) {
			if (current) {
				current->next = new_trait;
				current = current->next;
			} else {
				traits = new_trait;
				current = new_trait;
			}
			new_trait = 0;
		}
	}
	pseudo_debug_flags_finalize();

	if (optind < argc) {
		pseudo_diag("Error: Extra arguments not associated with any option.\n");
		usage(EXIT_FAILURE);
	}

	if (query_only && opt_l) {
		pseudo_diag("Error: -l cannot be used with query-only options or flags.\n");
		bad_args = 1;
	}

	/* should be set only if we have already diagnosed the bad arguments. */
	if (bad_args)
		exit(EXIT_FAILURE);

	if (!pseudo_get_prefix(argv[0])) {
		pseudo_diag("Can't figure out prefix.  Set PSEUDO_PREFIX or invoke with full path.\n");
		exit(EXIT_FAILURE);
	}

	if (!pseudo_get_bindir()) {
		pseudo_diag("Can't figure out bindir.  Set PSEUDO_BINDIR.\n");
		exit(EXIT_FAILURE);
	}

	if (!pseudo_get_libdir()) {
		pseudo_diag("Can't figure out libdir.  Set PSEUDO_LIBDIR.\n");
		exit(EXIT_FAILURE);
	}

	if (!pseudo_get_localstatedir()) {
		pseudo_diag("Can't figure out localstatedir.  Set PSEUDO_LOCALSTATEDIR.\n");
		exit(EXIT_FAILURE);
	}

	if (opt_l) {
		pdb_log_traits(traits);
	} else {
		int fields;
		fields = format_scan(format);
		if (fields == -1) {
			pseudo_diag("couldn't parse format string (%s).\n", format);
			return EXIT_FAILURE;
		}
		if (opt_D) {
			if (pdb_delete(traits, fields)) {
				pseudo_diag("errors occurred trying to delete entries.\n");
			}
		} else {
			history = pdb_history(traits, fields, opt_U);
			if (history) {
				log_entry *e;
				while ((e = pdb_history_entry(history)) != NULL) {
					display(e, format);
					log_entry_free(e);
				}
				pdb_history_free(history);
			} else {
				pseudo_diag("could not retrieve history.\n");
				return EXIT_FAILURE;
			}
		}
	}
	return 0;
}

/* print a single member of log, based on a single format specifier;
 * returns the address of the last character of the format specifier.
 */
static char *
format_one(log_entry *e, char *format) {
	char fmtbuf[256];
	size_t len = strcspn(format, "acdfgGimMoprRsStTuy"), real_len;
	char scratch[4096];
	time_t stamp_sec;
	struct tm stamp_tm;
	char *s;

	if (!e || !format) {
		pseudo_diag("invalid log entry or format specifier.\n");
		return 0;
	}
	real_len = snprintf(fmtbuf, sizeof(fmtbuf), "%.*s", (int) len + 1, format);
	if (real_len >= sizeof(fmtbuf) - 1) {
		pseudo_diag("Format string way too long starting at %.10s",
			format - 1);
		return 0;
	}
	/* point to the last character */
	s = fmtbuf + real_len - 1;

	/* The * modifier for width or precision requires additional
	 * parameters -- this doesn't make sense here.
	 */
	if (strchr(fmtbuf, '*') || strchr(fmtbuf, 'l') || strchr(fmtbuf, 'h')) {
		pseudo_diag("Sorry, you can't use *, h, or l format modifiers.\n");
		return 0;
	}

	switch (*s) {
	case 'a':		/* PSQF_ACCESS */
		*scratch = '\0';
		if (e->access == -1) {
			strcpy(scratch, "invalid");
		} else if (e->access != 0) {
			if (e->access & PSA_READ) {
				strcpy(scratch, "r");
				if (e->access & PSA_WRITE)
					strcat(scratch, "+");
			} else if (e->access & PSA_WRITE) {
				if (e->access & PSA_APPEND) {
					strcpy(scratch, "a");
				} else {
					strcpy(scratch, "w");
				}
				if (e->access & PSA_READ)
					strcat(scratch, "+");
			} else if (e->access & PSA_EXEC) {
				strcpy(scratch, "x");
			}
			/* this should be impossible... should. */
			if (e->access & PSA_APPEND && !(e->access & PSA_WRITE)) {
				strcat(scratch, "?a");
			}
		} else {
			strcpy(scratch, "-");
		}
		strcpy(s, "s");
		printf(fmtbuf, scratch);
		break;
	case 'c':		/* PSQF_CLIENT */
		strcpy(s, "d");
		printf(fmtbuf, (int) e->client);
		break;
	case 'd':		/* PSQF_DEV */
		strcpy(s, "d");
		printf(fmtbuf, (int) e->dev);
		break;
	case 'f':		/* PSQF_FD */
		strcpy(s, "d");
		printf(fmtbuf, (int) e->fd);
		break;
	case 'g':		/* PSQF_GID */
		strcpy(s, "d");
		printf(fmtbuf, (int) e->gid);
		break;
	case 'G':		/* PSQF_TAG */
		strcpy(s, "s");
		printf(fmtbuf, e->tag ? e->tag : "");
		break;
	case 'i':		/* PSQF_INODE */
		strcpy(s, "llu");
		printf(fmtbuf, (unsigned long long) e->ino);
		break;
	case 'm':		/* PSQF_PERM */
		strcpy(s, "o");
		printf(fmtbuf, (unsigned int) e->mode & ALLPERMS);
		break;
	case 'M':		/* PSQF_MODE */
		strcpy(s, "o");
		printf(fmtbuf, (unsigned int) e->mode);
		break;
	case 'o':		/* PSQF_OP */
		strcpy(s, "s");
		printf(fmtbuf, pseudo_op_name(e->op));
		break;
	case 'p':		/* PSQF_PATH */
		strcpy(s, "s");
		printf(fmtbuf, e->path ? e->path : "");
		break;
	case 'r':		/* PSQF_RESULT */
		strcpy(s, "s");
		printf(fmtbuf, pseudo_res_name(e->result));
		break;
	case 'R':		/* PSQF_PROGRAM */
		strcpy(s, "s");
		printf(fmtbuf, e->program ? e->program : "");
		break;
	case 's':		/* PSQF_STAMP */
		strcpy(s, "s");
		stamp_sec = e->stamp;
		localtime_r(&stamp_sec, &stamp_tm);
		strftime(scratch, sizeof(scratch), timeformat, &stamp_tm);
		printf(fmtbuf, scratch);
		break;
	case 'S':		/* PSQF_SEVERITY */
		strcpy(s, "s");
		printf(fmtbuf, pseudo_sev_name(e->severity));
		break;
	case 't':		/* PSQF_FTYPE */
	strcpy(s, "s");
		if (S_ISREG(e->mode)) {
			strcpy(scratch, "file");
		} if (S_ISLNK(e->mode)) {
			strcpy(scratch, "link");
		} else if (S_ISDIR(e->mode)) {
			strcpy(scratch, "dir");
		} else if (S_ISFIFO(e->mode)) {
			strcpy(scratch, "fifo");
		} else if (S_ISBLK(e->mode)) {
			strcpy(scratch, "block");
		} else if (S_ISCHR(e->mode)) {
			strcpy(scratch, "char");
		} else {
			snprintf(scratch, sizeof(scratch), "?%o", (unsigned int) e->mode & S_IFMT);
		}
		printf(fmtbuf, scratch);
		break;
	case 'T':		/* PSQF_TEXT */
		strcpy(s, "s");
		printf(fmtbuf, e->text ? e->text : "");
		break;
	case 'u':		/* PSQF_UID */
		strcpy(s, "d");
		printf(fmtbuf, (int) e->uid);
		break;
	case 'y':		/* PSQF_TYPE */
		strcpy(s, "s");
		printf(fmtbuf, pseudo_msg_type_name(e->type));
		break;
	}
	return format + len;
}

static int
format_scan(char *format) {
	char *s;
	size_t len;
	int fields = 0;
	pseudo_query_field_t field;

	for (s = format; (s = strchr(s, '%')) != NULL; ++s) {
		len = strcspn(s, "acdfgGimMoprRsStTuy");
		s += len;
		if (!*s) {
			pseudo_diag("Unknown format: '%.3s'\n",
				(s - len));
			return -1;
		}
		field = opt_to_field[(unsigned char) *s];
		switch (field) {
		case PSQF_PERM:		/* FALLTHROUGH */
		case PSQF_FTYPE:
			fields |= (1 << PSQF_MODE);
			break;
		case PSQF_ACCESS:	/* FALLTHROUGH */
		case PSQF_CLIENT:	/* FALLTHROUGH */
		case PSQF_DEV:		/* FALLTHROUGH */
		case PSQF_FD:		/* FALLTHROUGH */
		case PSQF_GID:		/* FALLTHROUGH */
		case PSQF_TAG:		/* FALLTHROUGH */
		case PSQF_INODE:	/* FALLTHROUGH */
		case PSQF_MODE:		/* FALLTHROUGH */
		case PSQF_OP:		/* FALLTHROUGH */
		case PSQF_PATH:		/* FALLTHROUGH */
		case PSQF_PROGRAM:	/* FALLTHROUGH */
		case PSQF_RESULT:	/* FALLTHROUGH */
		case PSQF_STAMP:	/* FALLTHROUGH */
		case PSQF_SEVERITY:	/* FALLTHROUGH */
		case PSQF_TEXT:		/* FALLTHROUGH */
		case PSQF_TYPE:		/* FALLTHROUGH */
		case PSQF_UID:
			fields |= (1 << field);
			break;
		case '\0':
			/* if there are no more formats, that may be wrong, but
			 * we can ignore it here
			 */
			break;
		default:
			pseudo_diag("error: invalid format specifier %c (at %s)\n", *s, s);
			return -1;
			break;
		}
	}
	return fields;
}

static void
display(log_entry *e, char *format) {
	for (; *format; ++format) {
		switch (*format) {
		case '%':
			format = format_one(e, format);
			if (!format)
				return;
			break;
		default:
			putchar(*format);
			break;
		}
	}
	putchar('\n');
}
