/***************************************************************************
*   Copyright (C) 2003 by Usarin Heininga                                 *
*   usarin@heininga.nl                                                    *
*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License as published by  *
*   the Free Software Foundation; either version 2 of the License, or     *
*   (at your option) any later version.                                   *
***************************************************************************/

#ifndef __LNC_USER_H__
#define __LNC_USER_H__
#ifdef __cplusplus
extern "C" {
#endif

#include <sys/types.h>
#include <pwd.h>
#include <limits.h>

	struct auth {
		char username[ NAME_MAX + 1 ];
		char password[ NAME_MAX + 1 ];
	};

//	void freeAuthorizationInfo( struct auth * auth );
	struct auth *newAuthorizationInfo( void );
	int readAuthorizationInfo( const char * URL, struct auth * auth );
	int writeAuthorizationInfo( const char * URL, struct auth * auth );
	int requestAuthorizationDialog( const char * URL, struct auth * auth );

	int getUIDbyPID( pid_t PID, uid_t * UID );
	int getGIDbyPID( pid_t PID, gid_t * GID );
	char *getTTYDeviceByPID( pid_t PID, char *buffer, size_t size );
	struct passwd *getSessionInfoByUID( uid_t sessionUID, char *buffer );
	struct passwd *getSessionInfoByName( char * userName, char *buffer );
	int changeToUser( uid_t UID, gid_t GID );

#ifdef __cplusplus
}
#endif
#endif
