/* the unix port wants to know that real_stat() and
 * friends exist.  So they do. And because the Linux
 * port really uses stat64 for those...
 */
int
pseudo_stat(const char *path, struct stat *buf) {
	return real___xstat(_STAT_VER, path, buf);
}

int
pseudo_lstat(const char *path, struct stat *buf) {
	return real___lxstat(_STAT_VER, path, buf);
}

int
pseudo_fstat(int fd, struct stat *buf) {
	return real___fxstat(_STAT_VER, fd, buf);
}

int
pseudo_stat64(const char *path, struct stat64 *buf) {
	return real___xstat64(_STAT_VER, path, buf);
}

int
pseudo_lstat64(const char *path, struct stat64 *buf) {
	return real___lxstat64(_STAT_VER, path, buf);
}

int
pseudo_fstat64(int fd, struct stat64 *buf) {
	return real___fxstat64(_STAT_VER, fd, buf);
}
