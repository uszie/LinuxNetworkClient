/***************************************************************************
*   Copyright (C) 2003 by Usarin Heininga                                 *
*   usarin@heininga.nl                                                    *
*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License as published by  *
*   the Free Software Foundation; either version 2 of the License, or     *
*   (at your option) any later version.                                   *
***************************************************************************/

#ifndef __NETWORKCLIENT_DAEMON_H__
#define __NETWORKCLIENT_DAEMON_H__

#include "fusefs.h"

#include "list.h"
#include "pluginhelper.h"

#include <sys/types.h>
#include <paths.h>
#include <limits.h>

#define APP_MAX 100

char registryPath[ PATH_MAX ];
char mountPath[ PATH_MAX ];
char pidPath[ PATH_MAX ];

int mountPathLength;
FILE *pidFile;
extern int useFullNetworkName;

struct application {
	struct list list;
	char *name;
	char *cmdLine;
	int syscall;
	int previousSyscall;
	int isLibrary;
	int disabled;
	int fakeSyscall;
	u_int64_t syscallStartTime;
	u_int64_t previousSyscallStartTime;
	u_int64_t previousSyscallEndTime;
};

defineList( applicationList, struct application );
defineList( libraryList, struct application );

extern struct applicationList *applicationList;
extern struct libraryList *libraryList;
extern sigset_t blockMask;

int addApplicationEntry( const char *applicationName, const char *string );
int isDaemonProcess( struct syscallContext *context );
int networkclientCallback( const char *URL, struct auth *auth );

void readNetworkClientConfiguration( void );
void freeNetworkClientConfiguration( void );
int updateNetworkClientConfiguration( const char *path, const char *filename, int event );

void exitHandler( int sig );
void cleanUp( void );

int hasProperArguments( pid_t PID, struct application *application );

#endif
