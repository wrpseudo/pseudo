/* 
 * Copyright (c) 2008-2010 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * static int
 * wrap_fclose(FILE *fp) {
 *	int rc = -1;
 */

	if (!fp) {
		errno = EFAULT;
		return -1;
	}
	int fd = fileno(fp);
	pseudo_client_op(OP_CLOSE, 0, fd, -1, 0, 0);
	rc = real_fclose(fp);

/*	return rc;
 * }
 */
