/*
 * pseudo_wrappers.c, darwin pseudo wrappers
 *
 * Copyright (c) 2008-2011 Wind River Systems, Inc.
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
/* there's no fgetgrent_r or fgetpwent_r in Darwin */
int
pseudo_getpwent_r(struct passwd *pwbuf, char *buf, size_t buflen, struct passwd **pwbufp) {
	/* note that we don't wrap fgetpwent_r, since there's no path
	 * references in it.
	 */
	if (!pseudo_pwd) {
		errno = ENOENT;
		return -1;
	}
	return fgetpwent_r(pseudo_pwd, pwbuf, buf, buflen, pwbufp);
}

int 
pseudo_getgrent_r(struct group *gbuf, char *buf, size_t buflen, struct group **gbufp) {
	/* note that we don't wrap fgetgrent_r, since there's no path
	 * references in it.
	 */
	if (!pseudo_grp) {
		errno = ENOENT;
		return -1;
	}
	return fgetgrent_r(pseudo_grp, gbuf, buf, buflen, gbufp);
}

#define PLENTY_LONG 2048
/* the original uid/gid code for Linux was written in terms of the
 * fget*ent_r() functions... which Darwin doesn't have.  But wait!  They're
 * actually pretty easy to implement.
 */
int
pseudo_fgetgrent_r(FILE *fp, struct group *gbuf, char *buf, size_t buflen, struct group **gbufp) {
	char linebuf[PLENTY_LONG] = { 0 };
	char *s, *t, *u;
	size_t max_members;
	char **members;
	size_t member = 0;
	long started_at = -1;
	gid_t gid;
	int error = ENOENT;
	size_t len;

	/* any early exit should set *gbufp to NULL */
	if (gbufp)
		*gbufp = NULL;

	if (!gbuf || !fp || !buf)
		goto error_out;

	started_at = ftell(fp);
	if (started_at == -1) {
		goto error_out;
	}
	s = fgets(linebuf, PLENTY_LONG, fp);
	if (!s) {
		goto error_out;
	}
	/* fgets will have stored a '\0' if there was no error; if there
	 * was an error, though, linebuf was initialized to all zeroes so
	 * the string is null-terminated anyway...
	 */
 	len = strlen(linebuf);
	if (len > buflen) {
		error = ERANGE;
		goto error_out;
	}
	memcpy(buf, linebuf, len);
	/* round up to 8, hope for the best? */
	len = len + 8 + (((unsigned long long) (buf + len)) % 8);
	members = (char **) (buf + len);
	if (len >= buflen) {
		error = ERANGE;
		goto error_out;
	}
	/* this is how many pointers we have room for... */
	max_members = (buflen - len) / sizeof(*members);

	t = buf;
	/* yes, I can assume that Darwin has strsep() */
	s = strsep(&t, ":");
	if (!s) {
		goto error_out;
	}
	gbuf->gr_name = s;
	s = strsep(&t, ":");
	if (!s) {
		goto error_out;
	}
	gbuf->gr_passwd = s;
	s = strsep(&t, ":");
	if (!s) {
		goto error_out;
	}
	gid = (gid_t) strtol(s, &u, 10);
	/* should be a null byte, otherwise we didn't get a valid number */
	if (*u)
		goto error_out;
	gbuf->gr_gid = gid;

	/* now, s points to a comma-separated list of members, which we
	 * want to stash pointers to in 'members'.
	 */
	s = strsep(&t, ":");
	t = s;
	while ((s = strsep(&t, ",")) != NULL) {
		if (*s) {
			if (member + 1 > max_members) {
				errno = ERANGE;
				goto error_out;
			}
			members[member++] = s;
		}
	}
	if (member + 1 > max_members) {
		errno = ERANGE;
		goto error_out;
	}
	members[member++] = NULL;
	*gbufp = gbuf;
	return 0;

error_out:
	if (started_at != -1)
		fseek(fp, started_at, SEEK_SET);
	return error;
	return -1;
}

int
pseudo_fgetpwent_r(FILE *fp, struct passwd *pbuf, char *buf, size_t buflen, struct passwd **pbufp) {
	char linebuf[PLENTY_LONG] = { 0 };
	char *s, *t, *u;
	long started_at = -1;
	__darwin_time_t timestamp;
	uid_t uid;
	gid_t gid;
	int error = ENOENT;
	size_t len;

	/* any early exit should set *gbufp to NULL */
	if (pbufp)
		*pbufp = NULL;

	if (!pbuf || !fp || !buf)
		goto error_out;

	started_at = ftell(fp);
	if (started_at == -1) {
		goto error_out;
	}
	s = fgets(linebuf, PLENTY_LONG, fp);
	if (!s) {
		goto error_out;
	}
	/* fgets will have stored a '\0' if there was no error; if there
	 * was an error, though, linebuf was initialized to all zeroes so
	 * the string is null-terminated anyway...
	 */
 	len = strlen(linebuf);
	if (len > buflen) {
		error = ERANGE;
		goto error_out;
	}
	memcpy(buf, linebuf, len);

	t = buf;
	/* yes, I can assume that Darwin has strsep() */
	s = strsep(&t, ":");
	if (!s) {
		goto error_out;
	}
	pbuf->pw_name = s;

	s = strsep(&t, ":");
	if (!s)
		goto error_out;
	pbuf->pw_passwd = s;

	s = strsep(&t, ":");
	if (!s)
		goto error_out;
	uid = (uid_t) strtol(s, &u, 10);
	/* should be a null byte, otherwise we didn't get a valid number */
	if (*u)
		goto error_out;
	pbuf->pw_uid = uid;

	s = strsep(&t, ":");
	if (!s)
		goto error_out;
	gid = (gid_t) strtol(s, &u, 10);
	/* should be a null byte, otherwise we didn't get a valid number */
	if (*u)
		goto error_out;
	pbuf->pw_gid = gid;

	s = strsep(&t, ":");
	if (!s)
		goto error_out;
	pbuf->pw_class = s;

	s = strsep(&t, ":");
	if (!s)
		goto error_out;
	timestamp = (__darwin_time_t) strtol(s, &u, 10);
	/* should be a null byte, otherwise we didn't get a valid number */
	if (*u)
		goto error_out;
	pbuf->pw_change = timestamp;

	timestamp = (__darwin_time_t) strtol(s, &u, 10);
	/* should be a null byte, otherwise we didn't get a valid number */
	if (*u)
		goto error_out;
	pbuf->pw_expire = timestamp;

	s = strsep(&t, ":");
	if (!s)
		goto error_out;
	pbuf->pw_gecos = s;

	s = strsep(&t, ":");
	if (!s)
		goto error_out;
	pbuf->pw_dir = s;

	s = strsep(&t, ":");
	if (!s)
		goto error_out;
	pbuf->pw_shell = s;

	*pbufp = pbuf;
	return 0;

error_out:
	if (started_at != -1)
		fseek(fp, started_at, SEEK_SET);
	return error;
	return -1;
}
