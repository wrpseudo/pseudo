/* there's no fgetgrent_r or fgetpwent_r in Darwin */

int
pseudo_fgetgrent_r(FILE *fp, struct group *gbuf, char *buf, size_t buflen, struct group **gbufp) {
	return -1;
}

int
pseudo_fgetpwent_r(FILE *fp, struct passwd *gbuf, char *buf, size_t buflen, struct passwd **pbufp) {
	return -1;
}
