/***************************************************************************
*   Copyright (C) 2003 by Usarin Heininga                                 *
*   usarin@heininga.nl                                                    *
*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License as published by  *
*   the Free Software Foundation; either version 2 of the License, or     *
*   (at your option) any later version.                                   *
***************************************************************************/

#ifndef __NETWORKCLIENT_FUSEFS_H__
#define __NETWORKCLIENT_FUSEFS_H__

#include <limits.h>
#include <fuse/fuse.h>

#define NETWORKCLIENT_STAT 1
//#define NETWORKCLIENT_OPEN 2
#define NETWORKCLIENT_OPENDIR 4

struct fuse *fuse;
struct syscallContext {
	pid_t PID;
	uid_t UID;
	gid_t GID;
	int syscall;
	long int syscallID;
	int markForUpdate;
	char applicationName[ NAME_MAX + 1 ];
	struct application *application;
};

//extern struct stat serviceAttributes;

struct syscallContext *getSyscallContext( void );

int fuseMount( const char *mountPoint );
void updateApplicationContext( struct syscallContext *context );

#endif
