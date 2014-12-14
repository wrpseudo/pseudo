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

/* There were symbol changes that can cause the linker to request
 * newer versions of glibc, which causes problems occasionally on
 * older hosts if pseudo is built against a newer glibc and then
 * run with an older one. Sometimes we can just avoid the symbols,
 * but memcpy's pretty hard to get away from.
 */
#define GLIBC_COMPAT_SYMBOL(sym, ver) __asm(".symver " #sym "," #sym "@GLIBC_" #ver)

#ifdef __amd64__   
GLIBC_COMPAT_SYMBOL(memcpy,2.2.5);
#elif defined(__i386__)
GLIBC_COMPAT_SYMBOL(memcpy,2.0);
#endif
