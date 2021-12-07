/***************************************************************************
*   Copyright (C) 2003 by Usarin Heininga                                 *
*   usarin@heininga.nl                                                    *
*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License as published by  *
*   the Free Software Foundation; either version 2 of the License, or     *
*   (at your option) any later version.                                   *
***************************************************************************/

#ifndef __NETWORKCLIENT_SESSION_H__
#define __NETWORKCLIENT_SESSION_H__

#include "list.h"

#include <pthread.h>

struct session {
	struct list list;
	pid_t sessionPID;
	uid_t sessionUID;
	gid_t sessionGID;
	u_int64_t timeStamp;
	char *userName;
	char *homeDirectory;
};
typedef struct session Session;

defineList( sessionList, struct session );
extern struct sessionList *sessionList;
extern int shareIconLength[ 2 ];
extern int hostIconLength[ 2 ];

char *createIconBuffer( unsigned int flags );

void createSessionList( void );
int updateSessions( const char *path, const char *filename, int event );
Session *sessionNew( uid_t sessionUID, gid_t sessionGID, char *UserName, char *HomeDirectory );
int spawnSession( Session *session );

#endif

