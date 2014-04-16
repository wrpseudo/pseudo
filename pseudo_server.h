/*
 * pseudo_server.h, pseudo server declarations and definitions
 *
 * Copyright (c) 2008-2009 Wind River Systems, Inc.
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
extern int pseudo_server_start(int);
extern int pseudo_server_response(pseudo_msg_t *msg, const char *program, const char *tag, char **response_path, size_t *response_len);
extern int pseudo_server_timeout;
extern int opt_l;
