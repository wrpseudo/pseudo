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
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <limits.h>

#include "pseudo.h"
#include "pseudo_ipc.h"
#include "pseudo_db.h"

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
static int pseudo_append_element(char **newpath, size_t *allocated, char **current, const char *element, size_t elen, int leave_last);
static int pseudo_append_elements(char **newpath, size_t *allocated, char **current, const char *elements, size_t elen, int leave_last);
extern char **environ;
static ssize_t pseudo_max_pathlen = -1;
static ssize_t pseudo_sys_max_pathlen = -1;

char *pseudo_version = PSEUDO_VERSION;

void
pseudo_debug_terse(void) {
	if (max_debug_level > 0)
		--max_debug_level;
}

void
pseudo_debug_verbose(void) {
	++max_debug_level;
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
pseudo_append_element(char **pnewpath, size_t *pallocated, char **pcurrent, const char *element, size_t elen, int leave_this) {
	static int link_recursion = 0;
	size_t curlen, allocated;
	char *newpath, *current;
	struct stat64 buf;
	if (!pnewpath || !*pnewpath || !pallocated || !pcurrent || !*pcurrent || !element) {
		pseudo_diag("pseudo_append_element: invalid args.\n");
		return -1;
	}
	newpath = *pnewpath;
	allocated = *pallocated;
	current = *pcurrent;
	/* sanity-check:  ignore // or /./ */
	if (elen == 0 || (elen == 1 && *element == '.')) {
		return 1;
	}
	/* backtrack for .. */
	if (elen == 2 && element[0] == '.' && element[1] == '.') {
		/* if newpath's whole contents are '/', do nothing */
		if (current <= newpath + 1)
			return 1;
		/* backtrack to the character before the / */
		current -= 2;
		/* now find the previous slash */
		while (current > newpath && *current != '/') {
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
		free(newpath);
		newpath = bigger;
		allocated = big;
		*pnewpath = newpath;
		*pcurrent = current;
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
		is_link = (lstat64(newpath, &buf) != -1) && S_ISLNK(buf.st_mode);
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
			retval = pseudo_append_elements(pnewpath, pallocated, pcurrent, linkbuf, linklen, 0);
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
pseudo_append_elements(char **newpath, size_t *allocated, char **current, const char *element, size_t elen, int leave_last) {
	int retval = 1;
	const char * start = element;
	if (!newpath || !current || !element || !*newpath || !*current) {
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
				if (pseudo_append_element(newpath, allocated, current, element, this_elen, leave_this) == -1) {
					retval = -1;
				}
			}
			break;
		default:
			if (pseudo_append_element(newpath, allocated, current, element, this_elen, leave_this) == -1) {
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
pseudo_fix_path(char *base, const char *path, size_t baselen, size_t *lenp, int leave_last) {
	size_t newpathlen, pathlen;
	char *newpath;
	char *current;
	
	if (!path) {
		pseudo_diag("can't fix empty path.\n");
		return 0;
	}
	pathlen = strlen(path);
	/* allow a bit of slush.  overallocating a bit won't
	 * hurt.  rounding to 256's in the hopes that it makes life
	 * easier for the library.
	 */
	if (path[0] == '/') {
		newpathlen = round_up(pathlen + 1, 256);
		newpath = malloc(newpathlen);
		if (!newpath) {
			pseudo_diag("allocation failed seeking memory for path (%s).\n", path);
			return 0;
		}
		current = newpath;
		++path;
	} else {
		newpathlen = round_up(baselen + pathlen + 2, 256);
		newpath = malloc(newpathlen);
		memcpy(newpath, base, baselen);
		current = newpath + baselen;
	}
	*current++ = '/';
	*current = '\0';
	/* at any given point:
	 * current points to just after the last / of newpath
	 * path points to the next path element of path
	 * newpathlen is the total allocated length of newpath
	 * (current - newpathlen) is the used length of newpath
	 * oldpath is the starting point of path
	 * (path - oldpath) is how far into path we are
	 */
	if (pseudo_append_elements(&newpath, &newpathlen, &current, path, pathlen, leave_last) != -1) {
		--current;
		if (*current == '/') {
			*current = '\0';
		}
		pseudo_debug(3, "%s + %s => <%s>\n",
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

/* remove the pseudo stuff from the environment (leaving other preloads
 * alone).
 */
void
pseudo_dropenv(void) {
	char *ld_env = getenv("LD_PRELOAD");

	/* why do this two ways?  Because calling setenv(), then execing,
	 * in bash seems to result in the variables still being set in the
	 * new environment.
	 * we remove the entire LD_PRELOAD, because our use case would have
	 * fakechroot and fakepasswd in it too -- and we don't want that.
	 * we don't touch LD_LIBRARY_PATH because it might be being used for
	 * other system libraries...
	 */
	if (ld_env) {
		unsetenv("LD_PRELOAD");
	}
}

/* add pseudo stuff to the environment.
 */
void
pseudo_setupenv(char *opts) {
	char *ld_env;
	char *newenv;
	size_t len;
	char debugvalue[64];

	newenv = "libpseudo.so";
	setenv("LD_PRELOAD", newenv, 1);

	ld_env = getenv("LD_LIBRARY_PATH");
	if (ld_env) {
		char *prefix = pseudo_prefix_path(NULL);
		if (!strstr(ld_env, prefix)) {
			char *e1, *e2;
			e1 = pseudo_prefix_path("lib");
			e2 = pseudo_prefix_path("lib64");
			len = strlen(ld_env) + strlen(e1) + strlen(e2) + 3;
			newenv = malloc(len);
			snprintf(newenv, len, "%s:%s:%s", ld_env, e1, e2);
			free(e1);
			free(e2);
			setenv("LD_LIBRARY_PATH", newenv, 1);
			free(newenv);
		}
		free(prefix);
	} else {
		char *e1, *e2;
		e1 = pseudo_prefix_path("lib");
		e2 = pseudo_prefix_path("lib64");
		len = strlen(e1) + strlen(e2) + 2;
		newenv = malloc(len);
		snprintf(newenv, len, "%s:%s", e1, e2);
		setenv("LD_LIBRARY_PATH", newenv, 1);
		free(newenv);
	}

	if (max_debug_level) {
		sprintf(debugvalue, "%d", max_debug_level);
		setenv("PSEUDO_DEBUG", debugvalue, 1);
	} else {
		unsetenv("PSEUDO_DEBUG");
	}

	if (opts) {
		setenv("PSEUDO_OPTS", opts, 1);
	} else {
		unsetenv("PSEUDO_OPTS");
	}
}

/* get the full path to a file under $PSEUDO_PREFIX.  Other ways of
 * setting the prefix all set it in the environment.
 */
char *
pseudo_prefix_path(char *file) {
	static char *prefix = NULL;
	static size_t prefix_len;
	char *path;

	if (!prefix) {
		prefix = getenv("PSEUDO_PREFIX");
		if (!prefix) {
			pseudo_diag("You must set the PSEUDO_PREFIX environment variable to run pseudo.\n");
			exit(1);
		}
		prefix_len = strlen(prefix);
	}
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
			if (rc >= len)
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

char *
pseudo_get_prefix(char *pathname) {
	char *s;
	s = getenv("PSEUDO_PREFIX");
	if (!s) {
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
		tmp_path = pseudo_fix_path(NULL, mypath, 0, 0, AT_SYMLINK_NOFOLLOW);
		/* point s to the end of the fixed path */
		if (strlen(tmp_path) >= pseudo_path_max()) {
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
		setenv("PSEUDO_PREFIX", mypath, 1);
		s = getenv("PSEUDO_PREFIX");
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
