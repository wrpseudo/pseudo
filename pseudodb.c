/*
 * pseudodb.c, (unimplemented) database maintenance utility.
 *
 * Copyright (c) 2008-2010 Wind River Systems, Inc.
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
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>

#include "pseudo.h"
#include "pseudo_ipc.h"
#include "pseudo_db.h"

int
main(int argc, char **argv) {
	pseudo_msg_t *msg;
	int rc;

	if (argc < 2) {
		fprintf(stderr, "Usage: pseudodb <filename>\n");
		exit(1);
	}
	msg = pseudo_msg_new(0, argv[1]);
	rc = pdb_find_file_path(msg, NULL);
	if (rc) {
		printf("error.\n");
		return 1;
	} else {
		printf("%s: %o %x\n", msg->path, (int) msg->mode, (int) msg->rdev);
		return 0;
	}
}
