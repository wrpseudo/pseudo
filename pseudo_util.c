/*
 * pseudo_util.c, miscellaneous utility functions
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
/* we need access to RTLD_NEXT for a horrible workaround */
#define _GNU_SOURCE

#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <regex.h>
#include <time.h>
#include <unistd.h>
#include <limits.h>

/* see the comments below about (*real_regcomp)() */
#include <dlfcn.h>

#include "pseudo.h"
#include "pseudo_ipc.h"
#include "pseudo_db.h"

struct pseudo_variables {
	char *key;
	size_t key_len;
	char *value;
};

/* The order below is not arbitrary, but based on an assumption
 * of how often things will be used.
 */
static struct pseudo_variables pseudo_env[] = {
	{ "PSEUDO_PREFIX", 13, NULL },
	{ "PSEUDO_BINDIR", 13, NULL },
	{ "PSEUDO_LIBDIR", 13, NULL },
	{ "PSEUDO_LOCALSTATEDIR", 20, NULL },
	{ "PSEUDO_PASSWD", 13, NULL },
	{ "PSEUDO_CHROOT", 13, NULL },
	{ "PSEUDO_UIDS", 11, NULL },
	{ "PSEUDO_GIDS", 11, NULL },
	{ "PSEUDO_OPTS", 11, NULL },
	{ "PSEUDO_DEBUG", 12, NULL },
	{ "PSEUDO_DEBUG_FILE", 17, NULL },
	{ "PSEUDO_TAG", 10, NULL },
	{ "PSEUDO_ENOSYS_ABORT", 19, NULL },
	{ "PSEUDO_NOSYMLINKEXP", 19, NULL },
	{ "PSEUDO_DISABLED", 15, NULL },
	{ "PSEUDO_UNLOAD", 13, NULL },
	{ NULL, 0, NULL } /* Magic terminator */
};

/* -1 - init hasn't been run yet
 * 0 - init has been run
 * 1 - init is running
 *
 * There are cases where the constructor is run AFTER the
 * program starts playing with things, so we need to do our
 * best to handle that case.
 */
static int pseudo_util_initted = -1;  /* Not yet run */

#if 0
static void
dump_env(char **envp) {
	size_t i = 0;
	for (i = 0; envp[i]; i++) {
		pseudo_debug(0,"dump_envp: [%d]%s\n", (int) i, envp[i]);
	}

	for (i = 0; pseudo_env[i].key; i++) {
		pseudo_debug(0,"dump_envp: {%d}%s=%s\n", (int) i, pseudo_env[i].key, pseudo_env[i].value);
	}

	pseudo_debug(0, "dump_envp: _in_init %d\n", pseudo_util_initted);
}
#endif

/* Caller must free memory! */
char *
pseudo_get_value(const char *key) {
	size_t i = 0;
	char * value;

	if (pseudo_util_initted == -1)
		pseudo_init_util();

	for (i = 0; pseudo_env[i].key && memcmp(pseudo_env[i].key, key, pseudo_env[i].key_len + 1); i++)
		;

	/* Check if the environment has it and we don't ...
	 * if so, something went wrong... so we'll attempt to recover
	 */
	if (pseudo_env[i].key && !pseudo_env[i].value && getenv(pseudo_env[i].key))
		pseudo_init_util();

	if (pseudo_env[i].value)
		value = strdup(pseudo_env[i].value);
	else
		value = NULL;

	if (!pseudo_env[i].key) 
		pseudo_diag("Unknown variable %s.\n", key);

	return value;
}

/* We make a copy, so the original values should be freed. */
int
pseudo_set_value(const char *key, const char *value) {
	int rc = 0;
	size_t i = 0;

	if (pseudo_util_initted == -1)
		pseudo_init_util();

	for (i = 0; pseudo_env[i].key && memcmp(pseudo_env[i].key, key, pseudo_env[i].key_len + 1); i++)
		;

	if (pseudo_env[i].key) {
		if (pseudo_env[i].value)
			free(pseudo_env[i].value);
		if (value) {
			char *new = strdup(value);
			if (new)
				pseudo_env[i].value = new;
			else
				pseudo_diag("warning: failed to save new value (%s) for key %s\n",
					value, key);
		} else
			pseudo_env[i].value = NULL;
	} else {
		if (!pseudo_util_initted) pseudo_diag("Unknown variable %s.\n", key);
		rc = -EINVAL;
	}

	return rc;
}

void
pseudo_init_util(void) {
	size_t i = 0;
	char * env;

	pseudo_util_initted = 1;

	for (i = 0; pseudo_env[i].key; i++) {
		if (getenv(pseudo_env[i].key))
			pseudo_set_value(pseudo_env[i].key, getenv(pseudo_env[i].key));
	}

	pseudo_util_initted = 0;

	/* Somewhere we have to set the debug level.. */
	env = pseudo_get_value("PSEUDO_DEBUG");
        if (env) {
		int i;
                int level = atoi(env);
                for (i = 0; i < level; ++i) {
                        pseudo_debug_verbose();
                }
        }
        free(env);
}

/* 5 = ridiculous levels of duplication
 * 4 = exhaustive detail
 * 3 = detailed protocol analysis
 * 2 = higher-level protocol analysis
 * 1 = stuff that might go wrong
 * 0 = fire and arterial bleeding
 */
static int max_debug_level = 0;
int pseudo_util_debug_fd = 2;
static int debugged_newline = 1;
static char pid_text[32];
static size_t pid_len;
static int pseudo_append_element(char **pnewpath, char **proot, size_t *pallocated, char **pcurrent, const char *element, size_t elen, int leave_this);
static int pseudo_append_elements(char **newpath, char **root, size_t *allocated, char **current, const char *elements, size_t elen, int leave_last);
extern char **environ;
static ssize_t pseudo_max_pathlen = -1;
static ssize_t pseudo_sys_max_pathlen = -1;

/* in our installed system, we usually use a name of the form
 * libpseudoCHECKSUM.so, where CHECKSUM is an md5 checksum of the host
 * libc.so -- this forces rebuilds of the library when the C library
 * changes.  The problem is that the pseudo binary may be
 * a prebuilt, in which case it doesn't know about CHECKSUM, so it
 * has to determine whether a given PRELINK_LIBRARIES contains libpseudo.so
 * or libpseudoCHECKSUM.so, without prior knowledge... Fancy!
 * 
 * We search for anything matching libpseudo*.so, where * is any
 * sequence of non-spaces (including an empty string), with either
 * the beginning of the string or a space in front of it, and either
 * the end of the string or a space after it.
 */
static char *libpseudo_name = "libpseudo.so";
/* this used to look for a "libpseudo*.so", but it turns out you can
 * specify a path even on Linux.
 */
static char *libpseudo_pattern = "(^|=| )[^ ]*libpseudo[^ ]*\\.so($| )";
static regex_t libpseudo_regex;
static int libpseudo_regex_compiled = 0;

/* Okay, so, there's a funny story behind this.  On one of the systems
 * we need to run on, /usr/bin/find happens to provide its own
 * definitions of regcomp and regexec which are INCOMPATIBLE with the
 * ones in the C library, and not only that, but which have buggy and/or
 * incompatible semantics, such that they trash elements of the pmatch
 * array.  So we do our best to call the "real" regcomp/regexec in the
 * C library.  If we can't find them, we just do our best and hope that
 * no one called us from a program with incompatible variants.
 *
 */
#if PSEUDO_PORT_LINUX
static int (*real_regcomp)(regex_t *__restrict __preg, const char *__restrict __pattern, int __cflags);
static int (*real_regexec)(const regex_t *__restrict __preg, const char *__restrict __string, size_t __nmatch, regmatch_t __pmatch[__restrict_arr], int __eflags);
#else
#define real_regcomp regcomp
#define real_regexec regexec
#endif /* PSEUDO_PORT_LINUX */

static int
libpseudo_regex_init(void) {
	int rc;

	if (libpseudo_regex_compiled)
		return 0;
#if PSEUDO_PORT_LINUX
	real_regcomp = dlsym(RTLD_NEXT, "regcomp");
	if (!real_regcomp)
		real_regcomp = regcomp;
	real_regexec = dlsym(RTLD_NEXT, "regexec");
	if (!real_regexec)
		real_regexec = regexec;
#endif
	rc = (*real_regcomp)(&libpseudo_regex, libpseudo_pattern, REG_EXTENDED);
	if (rc == 0)
		libpseudo_regex_compiled = 1;
	return rc;
}

/* given a space-or-colon-separated list of files, ala PRELINK_LIBRARIES,
 # return that list without any variants of libpseudo*.so.
 */
static char *
without_libpseudo(char *list) {
	regmatch_t pmatch[1];
	int counter = 0;
	int skip_start = 0;

	if (libpseudo_regex_init())
		return NULL;

	if (list[0] == '=' || list[0] == PSEUDO_LINKPATH_SEPARATOR[0])
		skip_start = 1;

	if ((*real_regexec)(&libpseudo_regex, list, 1, pmatch, 0)) {
		return list;
	}
	list = strdup(list);
	while (!(*real_regexec)(&libpseudo_regex, list, 1, pmatch, 0)) {
		char *start = list + pmatch[0].rm_so;
		char *end = list + pmatch[0].rm_eo;
		/* don't copy over the space or = */
		start += skip_start;
		memmove(start, end, strlen(end) + 1);
		++counter;
		if (counter > 5) {
			pseudo_diag("Found way too many libpseudo.so in environment, giving up.\n");
			return list;
		}
	}
	return list;
}

static char *
with_libpseudo(char *list, char *libdir_path) {
	regmatch_t pmatch[1];

	if (libpseudo_regex_init())
		return NULL;
	if ((*real_regexec)(&libpseudo_regex, list, 1, pmatch, 0)) {
		size_t len;
#if PSEUDO_PORT_DARWIN
		/* <%s:%s/%s\0> */
		len = strlen(list) + 1 + strlen(libdir_path) + 1 + strlen(libpseudo_name) + 1;
#else
		/* suppress warning */
		(void) libdir_path;
		/* <%s %s\0> */
		len = strlen(list) + 1 + strlen(libpseudo_name) + 1;
#endif
		char *new = malloc(len);
		if (new) {
			/* insert space only if there were previous bits */
			/* on Darwin, we have to provide the full path to
			 * libpseudo
			 */
#if PSEUDO_PORT_DARWIN
			snprintf(new, len, "%s%s%s/%s", list,
				*list ? PSEUDO_LINKPATH_SEPARATOR : "",
				libdir_path ? libdir_path : "",
				libpseudo_name);
#else
			snprintf(new, len, "%s%s%s", list,
				*list ? PSEUDO_LINKPATH_SEPARATOR : "",
				libpseudo_name);
#endif
		}
		return new;
	} else {
		return strdup(list);
	}
}

char *pseudo_version = PSEUDO_VERSION;

void
pseudo_debug_terse(void) {
	if (max_debug_level > 0)
		--max_debug_level;

	char s[16];
	snprintf(s, 16, "%d", max_debug_level);
	pseudo_set_value("PSEUDO_DEBUG", s);
}

void
pseudo_debug_verbose(void) {
	++max_debug_level;

	char s[16];
	snprintf(s, 16, "%d", max_debug_level);
	pseudo_set_value("PSEUDO_DEBUG", s);
}

int
pseudo_diag(char *fmt, ...) {
	va_list ap;
	char debuff[8192];
	int len;
	/* gcc on Ubuntu 8.10 requires that you examine the return from
	 * write(), and won't let you cast it to void.  Of course, if you
	 * can't print error messages, there's nothing to do.
	 */
	int wrote = 0;

	va_start(ap, fmt);
	len = vsnprintf(debuff, 8192, fmt, ap);
	va_end(ap);

	if (len > 8192)
		len = 8192;

	if (debugged_newline && max_debug_level > 1) {
		wrote += write(pseudo_util_debug_fd, pid_text, pid_len);
	}
	debugged_newline = (debuff[len - 1] == '\n');

	wrote += write(pseudo_util_debug_fd, "pseudo: ", 8);
	wrote += write(pseudo_util_debug_fd, debuff, len);
	return wrote;
}

int
pseudo_debug_real(int level, char *fmt, ...) {
	va_list ap;
	char debuff[8192];
	int len;
	int wrote = 0;

	if (max_debug_level < level)
		return 0;

	va_start(ap, fmt);
	len = vsnprintf(debuff, 8192, fmt, ap);
	va_end(ap);

	if (len > 8192)
		len = 8192;

	if (debugged_newline && max_debug_level > 1) {
		wrote += write(pseudo_util_debug_fd, pid_text, pid_len);
	}
	debugged_newline = (debuff[len - 1] == '\n');

	wrote += write(pseudo_util_debug_fd, debuff, len);
	return wrote;
}

/* given n, pick a multiple of block enough bigger than n
 * to give us some breathing room.
 */
static inline size_t
round_up(size_t n, size_t block) {
	return block * (((n + block / 4) / block) + 1);
}

/* store pid in text form for prepending to messages */
void
pseudo_new_pid() {
	pid_len = snprintf(pid_text, 32, "%d: ", getpid());
	pseudo_debug(2, "new pid.\n");
}

/* helper function for pseudo_fix_path
 * adds "element" to "newpath" at location current, if it can, then
 * checks whether this now points to a symlink.  If it does, expand
 * the symlink, appending each element in turn the same way.
 */
static int
pseudo_append_element(char **pnewpath, char **proot, size_t *pallocated, char **pcurrent, const char *element, size_t elen, int leave_this) {
	static int link_recursion = 0;
	size_t curlen, allocated;
	char *newpath, *current, *root;
	struct stat buf;
	if (!pnewpath || !*pnewpath ||
	    !pcurrent || !*pcurrent ||
	    !proot || !*proot ||
	    !pallocated || !element) {
		pseudo_diag("pseudo_append_element: invalid args.\n");
		return -1;
	}
	newpath = *pnewpath;
	allocated = *pallocated;
	current = *pcurrent;
	root = *proot;
	/* sanity-check:  ignore // or /./ */
	if (elen == 0 || (elen == 1 && *element == '.')) {
		return 1;
	}
	/* backtrack for .. */
	if (elen == 2 && element[0] == '.' && element[1] == '.') {
		/* if newpath's whole contents are '/', do nothing */
		if (current <= root + 1)
			return 1;
		/* backtrack to the character before the / */
		current -= 2;
		/* now find the previous slash */
		while (current > root && *current != '/') {
			--current;
		}
		/* and point to the nul just past it */
		*(++current) = '\0';
		*pcurrent = current;
		return 1;
	}
	curlen = current - newpath;
	/* current length, plus / <element> / \0 */
	/* => curlen + elen + 3 */
	if (curlen + elen + 3 > allocated) {
		char *bigger;
		size_t big = round_up(allocated + elen, 256);
		bigger = malloc(big);
		if (!bigger) {
			pseudo_diag("pseudo_append_element: couldn't allocate space (wanted %lu bytes).\n", (unsigned long) big);
			return -1;
		}
		memcpy(bigger, newpath, curlen);
		current = bigger + curlen;
		root = bigger + (root - newpath);
		free(newpath);
		newpath = bigger;
		allocated = big;
		*pnewpath = newpath;
		*pcurrent = current;
		*proot = root;
		*pallocated = allocated;
	}
	memcpy(current, element, elen);
	current += elen;
	/* nul-terminate, and we now point to the nul after the slash */
	*current = '\0';
	/* now, the moment of truth... is that a symlink? */
	/* if lstat fails, that's fine -- nonexistent files aren't symlinks */
	if (!leave_this) {
		int is_link;
		is_link = (lstat(newpath, &buf) != -1) && S_ISLNK(buf.st_mode);
		if (link_recursion >= PSEUDO_MAX_LINK_RECURSION && is_link) {
			pseudo_diag("link recursion too deep, not expanding path '%s'.\n", newpath);
			is_link = 0;
		}
		if (is_link) {
			char linkbuf[pseudo_path_max() + 1];
			ssize_t linklen;
			int retval;

			linklen = readlink(newpath, linkbuf, pseudo_path_max());
			if (linklen == -1) {
				pseudo_diag("uh-oh!  '%s' seems to be a symlink, but I can't read it.  Ignoring.", newpath);
				return 0;
			}
			/* null-terminate buffer */
			linkbuf[linklen] = '\0';
			/* absolute symlink means start over! */
			if (*linkbuf == '/') {
				current = newpath + 1;
			} else {
				/* point back at the end of the previous path... */
				current -= elen;
			}
			/* null terminate at the new pointer */
			*current = '\0';
			/* append all the elements in series */
			*pcurrent = current;
			++link_recursion;
			retval = pseudo_append_elements(pnewpath, proot, pallocated, pcurrent, linkbuf, linklen, 0);
			--link_recursion;
			return retval;
		}
	}
	/* okay, not a symlink, go ahead and append a slash */
	*(current++) = '/';
	*current = '\0';
	*pcurrent = current;
	return 1;
}

static int
pseudo_append_elements(char **newpath, char **root, size_t *allocated, char **current, const char *element, size_t elen, int leave_last) {
	int retval = 1;
	const char * start = element;
	if (!newpath || !*newpath ||
	    !root || !*root ||
	    !current || !*current ||
	    !element) {
		pseudo_diag("pseudo_append_elements: invalid arguments.");
		return -1;
	}
	while (element < (start + elen) && *element) {
		size_t this_elen;
		int leave_this = 0;
		char *next = strchr(element, '/');
		if (!next) {
			next = strchr(element, '\0');
			leave_this = leave_last;
		}
		this_elen = next - element;
		switch (this_elen) {
		case 0: /* path => '/' */
			break;
		case 1: /* path => '?/' */
			if (*element != '.') {
				if (pseudo_append_element(newpath, root, allocated, current, element, this_elen, leave_this) == -1) {
					retval = -1;
				}
			}
			break;
		default:
			if (pseudo_append_element(newpath, root, allocated, current, element, this_elen, leave_this) == -1) {
				retval = -1;
			}
			break;
		}
		/* and now move past the separator */
		element += this_elen + 1;
	}
	return retval;
}

/* Canonicalize path.  "base", if present, is an already-canonicalized
 * path of baselen characters, presumed not to end in a /.  path is
 * the new path to be canonicalized.  The tricky part is that path may
 * contain symlinks, which must be resolved.
 * if "path" starts with a /, then it is an absolute path, and
 * we ignore base.
 */
char *
pseudo_fix_path(const char *base, const char *path, size_t rootlen, size_t baselen, size_t *lenp, int leave_last) {
	size_t newpathlen, pathlen;
	char *newpath;
	char *current;
	char *effective_root;
	
	if (!path) {
		pseudo_diag("can't fix empty path.\n");
		return 0;
	}
	pathlen = strlen(path);
	newpathlen = pathlen;
	if (baselen && path[0] != '/') {
		newpathlen += baselen + 2;
	}
	/* allow a bit of slush.  overallocating a bit won't
	 * hurt.  rounding to 256's in the hopes that it makes life
	 * easier for the library.
	 */
	newpathlen = round_up(newpathlen, 256);
	newpath = malloc(newpathlen);
	if (!newpath) {
		pseudo_diag("allocation failed seeking memory for path (%s).\n", path);
		return 0;
	}
	current = newpath;
	if (baselen) {
		memcpy(current, base, baselen);
		current += baselen;
	}
	/* "root" is a pointer to the beginning of the *modifiable*
	 * part of the string; you can't back up over it.
	 */
	effective_root = newpath + rootlen;
	*current++ = '/';
	*current = '\0';
	/* at any given point:
	 * current points to just after the last / of newpath
	 * path points to the next element of path
	 * newpathlen is the total allocated length of newpath
	 * (current - newpath) is the used length of newpath
	 */
	if (pseudo_append_elements(&newpath, &effective_root, &newpathlen, &current, path, pathlen, leave_last) != -1) {
		--current;
		if (*current == '/' && current > effective_root) {
			*current = '\0';
		}
		pseudo_debug(5, "%s + %s => <%s>\n",
			base ? base : "<nil>",
			path ? path : "<nil>",
			newpath ? newpath : "<nil>");
		if (lenp) {
			*lenp = current - newpath;
		}
		return newpath;
	} else {
		free(newpath);
		return 0;
	}
}

/* remove the libpseudo stuff from the environment (leaving other preloads
 * alone).
 * There's an implicit memory leak here, but this is called only right
 * before an exec(), or at most once in a given run.
 *
 * we don't try to fix the library path.
 */
void pseudo_dropenv() {
	char *ld_preload = getenv(PRELINK_LIBRARIES);

	if (ld_preload) {
		ld_preload = without_libpseudo(ld_preload);
		if (!ld_preload) {
			pseudo_diag("fatal: can't allocate new %s variable.\n", PRELINK_LIBRARIES);
		}
		if (ld_preload && strlen(ld_preload)) {
			pseudo_diag("ld_preload without: <%s>\n", ld_preload);
			setenv(PRELINK_LIBRARIES, ld_preload, 1);
		} else {
			unsetenv(PRELINK_LIBRARIES);
		}
	}
}

char **
pseudo_dropenvp(char * const *envp) {
	char **new_envp;
	int i, j;

	for (i = 0; envp[i]; ++i) ;

	new_envp = malloc((i + 1) * sizeof(*new_envp));
	if (!new_envp) {
		pseudo_diag("fatal: can't allocate new environment.\n");
		return NULL;
	}

	j = 0;
	for (i = 0; envp[i]; ++i) {
		if (STARTSWITH(envp[i], PRELINK_LIBRARIES "=")) {
			char *new_val = without_libpseudo(envp[i]);
			if (!new_val) {
				pseudo_diag("fatal: can't allocate new environment variable.\n");
				return 0;
			} else {
				/* don't keep an empty value; if the whole string is
				 * PRELINK_LIRBARIES=, we just drop it. */
				if (strcmp(new_val, PRELINK_LIBRARIES "=")) {
					new_envp[j++] = new_val;
				}
			}
		} else {
			new_envp[j++] = envp[i];
		}
	}
	new_envp[j++] = NULL;
	return new_envp;
}

/* add pseudo stuff to the environment.
 */
void
pseudo_setupenv() {
	size_t i = 0;

	pseudo_debug(2, "setting up pseudo environment.\n");

	/* Make sure everything has been evaluated */
	free(pseudo_get_prefix(NULL));
	free(pseudo_get_bindir());
	free(pseudo_get_libdir());
	free(pseudo_get_localstatedir());

        while (pseudo_env[i].key) {
		if (pseudo_env[i].value)
			setenv(pseudo_env[i].key, pseudo_env[i].value, 0);
                i++;
        }

	const char *ld_library_path = getenv(PRELINK_PATH);
	char *libdir_path = pseudo_libdir_path(NULL);
	if (!ld_library_path) {
		size_t len = strlen(libdir_path) + 1 + (strlen(libdir_path) + 2) + 1;
		char *newenv = malloc(len);
		if (!newenv) {
			pseudo_diag("fatal: can't allocate new %s variable.\n", PRELINK_PATH);
		}
		snprintf(newenv, len, "%s:%s64", libdir_path, libdir_path);
		setenv(PRELINK_PATH, newenv, 1);
	} else if (!strstr(ld_library_path, libdir_path)) {
		size_t len = strlen(ld_library_path) + 1 + strlen(libdir_path) + 1 + (strlen(libdir_path) + 2) + 1;
		char *newenv = malloc(len);
		if (!newenv) {
			pseudo_diag("fatal: can't allocate new %s variable.\n", PRELINK_PATH);
		}
		snprintf(newenv, len, "%s:%s:%s64", ld_library_path, libdir_path, libdir_path);
		setenv(PRELINK_PATH, newenv, 1);
	} else {
		/* nothing to do, ld_library_path exists and contains
		 * our preferred path */
	}

	char *ld_preload = getenv(PRELINK_LIBRARIES);
	if (ld_preload) {
		ld_preload = with_libpseudo(ld_preload, libdir_path);
		if (!ld_preload) {
			pseudo_diag("fatal: can't allocate new %s variable.\n", PRELINK_LIBRARIES);
		}
		setenv(PRELINK_LIBRARIES, ld_preload, 1);
		free(ld_preload);
	} else {
		ld_preload = with_libpseudo("", libdir_path);
		if (!ld_preload) {
			pseudo_diag("fatal: can't allocate new %s variable.\n", PRELINK_LIBRARIES);
		}
		setenv(PRELINK_LIBRARIES, ld_preload, 1);
		free(ld_preload);
	}

	/* we kept libdir path until now because with_libpseudo might
	 * need it
	 */
	free(libdir_path);


#if PSEUDO_PORT_DARWIN
	char *force_flat = getenv("DYLD_FORCE_FLAT_NAMESPACE");
	if (!force_flat) {
		setenv("DYLD_FORCE_FLAT_NAMESPACE", "1", 1);
	}
#endif
}

/* add pseudo stuff to the environment.
 * We can't just use setenv(), because one use case is that we're trying
 * to modify the environment of a process about to be forked through
 * execve().
 */
char **
pseudo_setupenvp(char * const *envp) {
	char **new_envp;

	size_t i, j, k;
	size_t env_count = 0;

	size_t size_pseudoenv = 0;

	char *ld_preload = NULL, *ld_library_path = NULL;

	pseudo_debug(2, "setting up envp environment.\n");

	/* Make sure everything has been evaluated */
	free(pseudo_get_prefix(NULL));
	free(pseudo_get_bindir());
	free(pseudo_get_libdir());
	free(pseudo_get_localstatedir());

	for (i = 0; envp[i]; ++i) {
		if (STARTSWITH(envp[i], PRELINK_LIBRARIES "=")) {
			ld_preload = envp[i];
		}
		if (STARTSWITH(envp[i], PRELINK_PATH "=")) {
			ld_library_path = envp[i];
		}
		++env_count;
	}

        for (i = 0; pseudo_env[i].key; i++) {
		size_pseudoenv++;
        }

	env_count += size_pseudoenv; /* We're going to over allocate */

	j = 0;
	new_envp = malloc((env_count + 1) * sizeof(*new_envp));
	if (!new_envp) {
		pseudo_diag("fatal: can't allocate new environment.\n");
		return NULL;
	}	

	char *libdir_path = pseudo_libdir_path(NULL);
	if (!ld_library_path) {
		size_t len = strlen(PRELINK_PATH "=") + strlen(libdir_path) + 1 + (strlen(libdir_path) + 2) + 1;
		char *newenv = malloc(len);
		if (!newenv) {
			pseudo_diag("fatal: can't allocate new %s variable.\n", PRELINK_PATH);
		}
		snprintf(newenv, len, PRELINK_PATH "=%s:%s64", libdir_path, libdir_path);
		new_envp[j++] = newenv;
	} else if (!strstr(ld_library_path, libdir_path)) {
		size_t len = strlen(ld_library_path) + 1 + strlen(libdir_path) + 1 + (strlen(libdir_path) + 2) + 1;
		char *newenv = malloc(len);
		if (!newenv) {
			pseudo_diag("fatal: can't allocate new %s variable.\n", PRELINK_PATH);
		}
		snprintf(newenv, len, "%s:%s:%s64", ld_library_path, libdir_path, libdir_path);
		new_envp[j++] = newenv;
	} else {
		/* keep old value */
		new_envp[j++] = ld_library_path;
	}

	if (ld_preload) {
		ld_preload = with_libpseudo(ld_preload, libdir_path);
		if (!ld_preload) {
			pseudo_diag("fatal: can't allocate new %s variable.\n", PRELINK_LIBRARIES);
		}
		new_envp[j++] = ld_preload;
	} else {
		ld_preload = with_libpseudo("", libdir_path);
		size_t len = strlen(PRELINK_LIBRARIES "=") + strlen(ld_preload) + 1;
		char *newenv = malloc(len);
		snprintf(newenv, len, PRELINK_LIBRARIES "=%s", ld_preload);
		new_envp[j++] = newenv;
		free(ld_preload);
	}

	free(libdir_path);

	for (i = 0; envp[i]; ++i) {
		if (STARTSWITH(envp[i], PRELINK_LIBRARIES "=")) continue;
		if (STARTSWITH(envp[i], PRELINK_PATH "=")) continue;
		new_envp[j++] = envp[i];
	}

	for (i = 0; pseudo_env[i].key; i++) {
		int found = 0;
		for (k = 0; k < j; k++) {
			if (!strncmp(pseudo_env[i].key,new_envp[k],strlen(pseudo_env[i].key))) {
				found = 1;
				break;
			}
		}
		if (!found && pseudo_env[i].key && pseudo_env[i].value) {
			size_t len = strlen(pseudo_env[i].key) + 1 + strlen(pseudo_env[i].value) + 1;
			char *newenv = malloc(len);
			if (!newenv) {
				pseudo_diag("fatal: can't allocate new variable.\n");
			}
			snprintf(newenv, len, "%s=%s", pseudo_env[i].key, pseudo_env[i].value);
			new_envp[j++] = newenv;
		}
	}
	new_envp[j++] = NULL;
	return new_envp;
}

/* Append the file value to the prefix value. */
char *
pseudo_append_path(const char * prefix, size_t prefix_len, char *file) {
	char *path;

	if (!file) {
		return strdup(prefix);
	} else {
		size_t len = prefix_len + strlen(file) + 2;
		path = malloc(len);
		if (path) {
			char *endptr;
			int rc;

			rc = snprintf(path, len, "%s", prefix);
			/* this certainly SHOULD be impossible */
			if ((size_t) rc >= len)
				rc = len - 1;
			endptr = path + rc;
			/* strip extra slashes.
			 * This probably has no real effect, but I don't like
			 * seeing "//" in paths.
			 */
			while ((endptr > path) && (endptr[-1] == '/'))
				--endptr;
			snprintf(endptr, len - (endptr - path), "/%s", file);
		}
		return path;
	}
}


/* get the full path to a file under $PSEUDO_PREFIX.  Other ways of
 * setting the prefix all set it in the environment.
 */
char *
pseudo_prefix_path(char *file) {
	char * rc;
	char * prefix = pseudo_get_prefix(NULL);

	if (!prefix) {
		pseudo_diag("You must set the PSEUDO_PREFIX environment variable to run pseudo.\n");
		exit(1);
	}

	rc = pseudo_append_path(prefix, strlen(prefix), file);	
	free(prefix);

	return rc;
}

/* get the full path to a file under $PSEUDO_BINDIR. */
char *
pseudo_bindir_path(char *file) {
	char * rc;
	char * bindir = pseudo_get_bindir();

	if (!bindir) {
		pseudo_diag("You must set the PSEUDO_BINDIR environment variable to run pseudo.\n");
		exit(1);
	}

	rc = pseudo_append_path(bindir, strlen(bindir), file);	
	free(bindir);

	return rc;
}

/* get the full path to a file under $PSEUDO_LIBDIR. */
char *
pseudo_libdir_path(char *file) {
	char * rc;
	char * libdir = pseudo_get_libdir();

	if (!libdir) {
		pseudo_diag("You must set the PSEUDO_LIBDIR environment variable to run pseudo.\n");
		exit(1);
	}

	rc = pseudo_append_path(libdir, strlen(libdir), file);	
	free(libdir);

	return rc;
}

/* get the full path to a file under $PSEUDO_LOCALSTATEDIR. */
char *
pseudo_localstatedir_path(char *file) {
	char * rc;
	char * localstatedir = pseudo_get_localstatedir();

	if (!localstatedir) {
		pseudo_diag("You must set the PSEUDO_LOCALSTATEDIR environment variable to run pseudo.\n");
		exit(1);
	}

	rc = pseudo_append_path(localstatedir, strlen(localstatedir), file);	
	free(localstatedir);

	return rc;
}

char *
pseudo_get_prefix(char *pathname) {
	char *s = pseudo_get_value("PSEUDO_PREFIX");

	/* Generate the PSEUDO_PREFIX if necessary, and possible... */
	if (!s && pathname) {
		char mypath[pseudo_path_max()];
		char *dir;
		char *tmp_path;

		if (pathname[0] == '/') {
			snprintf(mypath, pseudo_path_max(), "%s", pathname);
			s = mypath + strlen(mypath);
		} else {
			if (!getcwd(mypath, pseudo_path_max())) {
				mypath[0] = '\0';
			}
			s = mypath + strlen(mypath);
			s += snprintf(s, pseudo_path_max() - (s - mypath), "/%s",
				pathname);
		}
		tmp_path = pseudo_fix_path(NULL, mypath, 0, 0, 0, AT_SYMLINK_NOFOLLOW);
		/* point s to the end of the fixed path */
		if ((int) strlen(tmp_path) >= pseudo_path_max()) {
			pseudo_diag("Can't expand path '%s' -- expansion exceeds %d.\n",
				mypath, (int) pseudo_path_max());
			free(tmp_path);
		} else {
			s = mypath + snprintf(mypath, pseudo_path_max(), "%s", tmp_path);
			free(tmp_path);
		}

		while (s > (mypath + 1) && *s != '/')
			--s;
		*s = '\0';
		dir = s - 1;
		while (dir > mypath && *dir != '/') {
			--dir;
		}
		/* strip bin directory, if any */
		if (!strncmp(dir, "/bin", 4)) {
			*dir = '\0';
		}
		/* degenerate case: /bin/pseudo should yield a pseudo_prefix "/" */
		if (*mypath == '\0') {
			strcpy(mypath, "/");
		}

		pseudo_diag("Warning: PSEUDO_PREFIX unset, defaulting to %s.\n",
			mypath);
		pseudo_set_value("PSEUDO_PREFIX", mypath);
		s = pseudo_get_value("PSEUDO_PREFIX");
	}
	return s;
}

char *
pseudo_get_bindir(void) {
	char *s = pseudo_get_value("PSEUDO_BINDIR");
	if (!s) {
		char *pseudo_bindir = pseudo_prefix_path(PSEUDO_BINDIR);;
		if (pseudo_bindir) {
			pseudo_set_value("PSEUDO_BINDIR", pseudo_bindir);
			s = pseudo_bindir;
		}
	}
	return s;
}

char *
pseudo_get_libdir(void) {
	char *s = pseudo_get_value("PSEUDO_LIBDIR");
	if (!s) {
		char *pseudo_libdir;
		pseudo_libdir = pseudo_prefix_path(PSEUDO_LIBDIR);
		if (pseudo_libdir) {
			pseudo_set_value("PSEUDO_LIBDIR", pseudo_libdir);
			s = pseudo_libdir;
		}
	}
#if PSEUDO_PORT_DARWIN
	/* on Darwin, we need lib64, because dyld won't search */
#else
	/* If we somehow got lib64 in there, clean it down to just lib... */
	if (s) {
		size_t len = strlen(s);
		if (s[len-2] == '6' && s[len-1] == '4') {
			s[len-2] = '\0';
			pseudo_set_value("PSEUDO_LIBDIR", s);
		}
	}
#endif

	return s;
}

char *
pseudo_get_localstatedir() {
	char *s = pseudo_get_value("PSEUDO_LOCALSTATEDIR");
	if (!s) {
		char *pseudo_localstatedir = pseudo_prefix_path(PSEUDO_LOCALSTATEDIR);
		if (pseudo_localstatedir) {
			pseudo_set_value("PSEUDO_LOCALSTATEDIR", pseudo_localstatedir);
			s = pseudo_localstatedir;
		}
	}
	return s;
}

/* these functions define the sizes pseudo will try to use
 * when trying to allocate space, or guess how much space
 * other people will have allocated; see the GNU man page
 * for realpath(3) for an explanation of why the sys_path_max
 * functions exists, approximately -- it's there to be a size
 * that I'm pretty sure the user will have allocated if they
 * provided a buffer to that defective function.
 */
/* I'm pretty sure this will be larger than real PATH_MAX */
#define REALLY_BIG_PATH 16384
/* A likely common value for PATH_MAX */
#define SORTA_BIG_PATH 4096
ssize_t
pseudo_path_max(void) {
	if (pseudo_max_pathlen == -1) {
		long l = pathconf("/", _PC_PATH_MAX);
		if (l < 0) {
			if (_POSIX_PATH_MAX > 0) {
				pseudo_max_pathlen = _POSIX_PATH_MAX;
			} else {
				pseudo_max_pathlen = REALLY_BIG_PATH;
			}
		} else {
			if (l <= REALLY_BIG_PATH) {
				pseudo_max_pathlen = l;
			} else {
				pseudo_max_pathlen = REALLY_BIG_PATH;
			}
		}
	}
	return pseudo_max_pathlen;
}

ssize_t
pseudo_sys_path_max(void) {
	if (pseudo_sys_max_pathlen == -1) {
		long l = pathconf("/", _PC_PATH_MAX);
		if (l < 0) {
			if (_POSIX_PATH_MAX > 0) {
				pseudo_sys_max_pathlen = _POSIX_PATH_MAX;
			} else {
				pseudo_sys_max_pathlen = SORTA_BIG_PATH;
			}
		} else {
			if (l <= SORTA_BIG_PATH) {
				pseudo_sys_max_pathlen = l;
			} else {
				pseudo_sys_max_pathlen = SORTA_BIG_PATH;
			}
		}
	}
	return pseudo_sys_max_pathlen;
}

/* complicated because in theory you can have modes like * 'ab+'
 * which is the same as 'a+' in POSIX.  The first letter really does have
 * to be one of r, w, a, though.
 */
int
pseudo_access_fopen(const char *mode) {
	int access = 0;
	for (; *mode; ++mode) {
		switch (*mode) {
		case 'a':
			access |= (PSA_APPEND | PSA_WRITE);
			break;
		case 'r':
			access |= PSA_READ;
			break;
		case 'w':
			access |= PSA_WRITE;
			break;
		case 'x':
			/* special case -- note that this conflicts with a
			 * rarely-used glibc extension
			 */
			access |= PSA_EXEC;
			break;
		case 'b':			/* binary mode */
			break;
		case 'c': case 'e': case 'm':	/* glibc extensions */
			break;
		case '+':
			/* one of these will already be set, presumably */
			access |= (PSA_READ | PSA_WRITE);
			break;
		default:
			access = -1;
			break;
		}
	}
	return access;
}

/* find a passwd/group file to use
 * uses in order:
 * - PSEUDO_CHROOT/etc/<file> (only if CHROOT is set)
 * - PSEUDO_PASSWD/etc/<file>
 * - /etc/<file>
 */

#if PSEUDO_PORT_DARWIN
/* on Darwin, you can't just use /etc/passwd for system lookups,
 * you have to use the real library calls because they know about
 * Directory Services.  So...
 *
 * We make up fake fds and FILE * objects that can't possibly be
 * valid.
 */
int pseudo_host_etc_passwd_fd = -3;
int pseudo_host_etc_group_fd = -4;
static FILE pseudo_fake_passwd_file;
static FILE pseudo_fake_group_file;
FILE *pseudo_host_etc_passwd_file = &pseudo_fake_passwd_file;
FILE *pseudo_host_etc_group_file = &pseudo_fake_group_file;

#endif

int
pseudo_etc_file(const char *file, char *realname, int flags, char **search_dirs, int dircount) {
	char filename[pseudo_path_max()];
	int rc;

	if (!file) {
		pseudo_diag("pseudo_etc_file: needs argument, usually passwd/group\n");
		return 0;
	}
	if (search_dirs) {
		char *s;
		int i;
		for (i = 0; i < dircount; ++i) {
			s = search_dirs[i];
			if (!s)
				continue;
			snprintf(filename, pseudo_path_max(), "%s/etc/%s",
				s, file);
			if (flags & O_CREAT) {
				rc = open(filename, flags, 0600);
			} else {
				rc = open(filename, flags);
			}
			if (rc >= 0) {
				if (realname)
					strcpy(realname, filename);
				pseudo_debug(2, "using <%s> for <%s>\n",
					filename, file);
				return rc;
			} else {
				pseudo_debug(3, "didn't find <%s>\n",
					filename);
			}
		}
	} else {
		pseudo_debug(2, "pseudo_etc_file: no search dirs.\n");
	}
#if PSEUDO_PORT_DARWIN
	if (!strcmp("passwd", file)) {
		pseudo_debug(2, "Darwin hackery: pseudo_etc_passwd returning magic passwd fd\n");
		return pseudo_host_etc_passwd_fd;
	} else if (!strcmp("group", file)) {
		pseudo_debug(2, "Darwin hackery: pseudo_etc_passwd returning magic group fd\n");
		return pseudo_host_etc_group_fd;
	}
#endif
	snprintf(filename, pseudo_path_max(), "/etc/%s", file);
	pseudo_debug(2, "falling back on <%s> for <%s>\n",
		filename, file);
	if (flags & O_CREAT) {
		rc = open(filename, flags, 0600);
	} else {
		rc = open(filename, flags);
	}
	if (rc >= 0 && realname)
		strcpy(realname, filename);
	return rc;
}

/* set up a log file */
int
pseudo_logfile(char *defname) {
	char *pseudo_path;
	char *filename = pseudo_get_value("PSEUDO_DEBUG_FILE");
	char *s;
#if PSEUDO_PORT_LINUX
	extern char *program_invocation_short_name; /* glibcism */
#else
	char *program_invocation_short_name = "unknown";
#endif
	int fd;

	if (!filename) {
		if (!defname) {
			pseudo_debug(3, "no special log file requested, using stderr.\n");
			return -1;
		}
		pseudo_path = pseudo_localstatedir_path(defname);
		if (!pseudo_path) {
			pseudo_diag("can't get path for prefix/%s\n", PSEUDO_LOGFILE);
			return -1;
		}
	} else {
		char *pid = NULL, *prog = NULL;
		size_t len;
		for (s = filename; *s; ++s) {
			if (s[0] == '%') {
				switch (s[1]) {
				case '%': /* skip the %% */
					++s;
					break;
				case 'd':
					if (pid) {
						pseudo_diag("found second %%d in PSEUDO_DEBUG_FILE, ignoring.\n");
						return -1;
					} else {
						pid = s;
					}
					break;
				case 's':
					if (prog) {
						pseudo_diag("found second %%s in PSEUDO_DEBUG_FILE, ignoring.\n");
						return -1;
					} else {
						prog = s;
					}
					break;
				default:
					if (isprint(s[1])) {
						pseudo_diag("found unknown format character '%c' in PSEUDO_DEBUG_FILE, ignoring.\n",
							s[1]);
					} else {
						pseudo_diag("found unknown format character '\\x%02x' in PSEUDO_DEBUG_FILE, ignoring.\n",
							(unsigned char) s[1]);
					}
					return -1;
					break;
				}
			}
		}
		len = strlen(filename) + 1;
		if (pid)
			len += 8;
		if (prog)
			len += strlen(program_invocation_short_name);
		pseudo_path = malloc(len);
		if (!pseudo_path) {
			pseudo_diag("can't allocate space for debug file name.\n");
			return -1;
		}
		if (pid && prog) {
			if (pid < prog) {
				snprintf(pseudo_path, len, filename, getpid(), program_invocation_short_name);
			} else {
				snprintf(pseudo_path, len, filename, program_invocation_short_name, getpid());
			}
		} else if (pid) {
			snprintf(pseudo_path, len, filename, getpid());
		} else if (prog) {
			snprintf(pseudo_path, len, filename, program_invocation_short_name);
		} else {
			strcpy(pseudo_path, filename);
		}
		free(filename);
	}	
	fd = open(pseudo_path, O_WRONLY | O_APPEND | O_CREAT, 0644);
	if (fd == -1) {
		pseudo_diag("help: can't open log file %s: %s\n", pseudo_path, strerror(errno));
	} else {
		/* try to force fd to 2.  We do this because glibc's malloc
		 * debug unconditionally writes to fd 2, and we don't want
		 * a client process ending op on fd 2, or server debugging
		 * becomes a nightmare.
		 */
		if (fd != 2) {
			int newfd;
			close(2);
			newfd = dup2(fd, 2);
			if (newfd != -1) {
				fd = newfd;
			}
		}
		pseudo_util_debug_fd = fd;
	}
	free(pseudo_path);
	if (fd == -1)
		return -1;
	else
		return 0;
}

void
pseudo_stat32_from64(struct stat *buf32, const struct stat64 *buf) {
	buf32->st_dev = buf->st_dev;
	buf32->st_ino = buf->st_ino;
	buf32->st_mode = buf->st_mode;
	buf32->st_nlink = buf->st_nlink;
	buf32->st_uid = buf->st_uid;
	buf32->st_gid = buf->st_gid;
	buf32->st_rdev = buf->st_rdev;
	buf32->st_size = buf->st_size;
	buf32->st_blksize = buf->st_blksize;
	buf32->st_blocks = buf->st_blocks;
	buf32->st_atime = buf->st_atime;
	buf32->st_mtime = buf->st_mtime;
	buf32->st_ctime = buf->st_ctime;
}

void
pseudo_stat64_from32(struct stat64 *buf64, const struct stat *buf) {
	buf64->st_dev = buf->st_dev;
	buf64->st_ino = buf->st_ino;
	buf64->st_mode = buf->st_mode;
	buf64->st_nlink = buf->st_nlink;
	buf64->st_uid = buf->st_uid;
	buf64->st_gid = buf->st_gid;
	buf64->st_rdev = buf->st_rdev;
	buf64->st_size = buf->st_size;
	buf64->st_blksize = buf->st_blksize;
	buf64->st_blocks = buf->st_blocks;
	buf64->st_atime = buf->st_atime;
	buf64->st_mtime = buf->st_mtime;
	buf64->st_ctime = buf->st_ctime;
}
