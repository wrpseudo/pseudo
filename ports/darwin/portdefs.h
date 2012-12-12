#define PRELINK_LIBRARIES "DYLD_INSERT_LIBRARIES"
#define PRELINK_PATH "DYLD_LIBRARY_PATH"
#define PSEUDO_STATBUF_64 0
#define PSEUDO_STATBUF struct stat
#define PSEUDO_LINKPATH_SEPARATOR ":"
/* hackery to allow sneaky things to be done with getgrent() */
extern int pseudo_host_etc_passwd_fd;
extern int pseudo_host_etc_group_fd;
extern FILE *pseudo_host_etc_passwd_file;
extern FILE *pseudo_host_etc_group_file;
/* Darwin ALWAYS follows symlinks for link(2) */
#undef PSEUDO_LINK_SYMLINK_BEHAVIOR
#define PSEUDO_LINK_SYMLINK_BEHAVIOR AT_SYMLINK_FOLLOW
