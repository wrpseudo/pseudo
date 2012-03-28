/*
 * Copyright (c) 2011, 2012 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * int system(const char *command)
 *	int rc = -1;
 */
	if (!command)
		return 1;

	pseudo_setupenv();
	if (pseudo_get_value("PSEUDO_UNLOAD"))
		pseudo_dropenv();

	rc = real_system(command);

/*	return rc;
 * }
 */
