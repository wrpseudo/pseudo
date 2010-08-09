/* 
 * Copyright (c) 2010 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * static int
 * wrap_execle(const char *path, const char *arg, va_list ap) {
 *	int rc = -1;
 */

  size_t i = 0;
  size_t alloc_size = 256;
  const char **argv = malloc(sizeof (const char *) * alloc_size);
  char *const *envp;

  argv[i++] = arg;

  while (argv[i-1]) {
	argv[i++] = va_arg (ap, const char *);
	if ( i > alloc_size - 1 ) {
		alloc_size = alloc_size + 256;
		argv = realloc(argv, sizeof (const char *) * alloc_size);
	}
  }
  envp = va_arg (ap, char *const *);

  rc = wrap_execve (path, (char *const *) argv, envp);

  free (argv);

/*	return rc;
 * }
 */
