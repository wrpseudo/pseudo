/*
 * offsets.c, print offsets in pseudo_ipc structure
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
#define _LARGEFILE64_SOURCE
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stddef.h>

#include "pseudo.h"
#include "pseudo_ipc.h"

int
main(void) {
	printf("type: %d\n", (int) offsetof(pseudo_msg_t, type));
	printf("op: %d\n", (int) offsetof(pseudo_msg_t, op));
	printf("result: %d\n", (int) offsetof(pseudo_msg_t, result));
	printf("access: %d\n", (int) offsetof(pseudo_msg_t, access));
	printf("client: %d\n", (int) offsetof(pseudo_msg_t, client));
	printf("dev: %d\n", (int) offsetof(pseudo_msg_t, dev));
	printf("ino: %d\n", (int) offsetof(pseudo_msg_t, ino));
	printf("uid: %d\n", (int) offsetof(pseudo_msg_t, uid));
	printf("gid: %d\n", (int) offsetof(pseudo_msg_t, gid));
	printf("mode: %d\n", (int) offsetof(pseudo_msg_t, mode));
	printf("rdev: %d\n", (int) offsetof(pseudo_msg_t, rdev));
	printf("pathlen: %d\n", (int) offsetof(pseudo_msg_t, pathlen));
	printf("nlink: %d\n", (int) offsetof(pseudo_msg_t, nlink));
	printf("deleting: %d\n", (int) offsetof(pseudo_msg_t, deleting));
	printf("path: %d\n", (int) offsetof(pseudo_msg_t, path));
	printf("size: %d\n", (int) sizeof(pseudo_msg_t));
	return 0;
}

