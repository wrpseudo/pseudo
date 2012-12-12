#define PRELINK_LIBRARIES "LD_PRELOAD"
#define PRELINK_PATH "LD_LIBRARY_PATH"
#define PSEUDO_STATBUF_64 1
#define PSEUDO_STATBUF struct stat64
#define PSEUDO_LINKPATH_SEPARATOR " "
/* Linux NEVER follows symlinks for link(2)... except on old kernels
 * I don't care about.
 */
#undef PSEUDO_LINK_SYMLINK_BEHAVIOR
/* Note: 0, here, really means AT_SYMLINK_NOFOLLOW, but specifying that
 * causes errors; you have to leave it empty or specify AT_SYMLINK_FOLLOW.
 */
#define PSEUDO_LINK_SYMLINK_BEHAVIOR 0
