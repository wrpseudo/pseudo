/* there's no fgetgrent_r or fgetpwent_r in Darwin */
int
pseudo_getpwent_r(struct passwd *pwbuf, char *buf, size_t buflen, struct passwd **pwbufp) {
	(void) pwbuf;
	(void) buf;
	(void) buflen;
	(void) pwbufp;
	return -1;
}
int 
pseudo_getgrent_r(struct group *gbuf, char *buf, size_t buflen, struct group **gbufp) {
	(void) gbuf;
	(void) buf;
	(void) buflen;
	(void) gbufp;
	return -1;
}

int
pseudo_fgetgrent_r(FILE *fp, struct group *gbuf, char *buf, size_t buflen, struct group **gbufp) {
	(void) fp;
	(void) gbuf;
	(void) buf;
	(void) buflen;
	(void) gbufp;
	return -1;
}

int
pseudo_fgetpwent_r(FILE *fp, struct passwd *pbuf, char *buf, size_t buflen, struct passwd **pbufp) {
	(void) fp;
	(void) pbuf;
	(void) buf;
	(void) buflen;
	(void) pbufp;
	return -1;
}
