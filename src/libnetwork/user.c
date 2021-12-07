/***************************************************************************
*   Copyright (C) 2003 by Usarin Heininga                                 *
*   usarin@heininga.nl                                                    *
*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License as published by  *
*   the Free Software Foundation; either version 2 of the License, or     *
*   (at your option) any later version.                                   *
***************************************************************************/
#include "nconfig.h"
#include "user.h"
#include "plugin.h"
#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pwd.h>
#include <dirent.h>
#include <sys/wait.h>
#include <sys/param.h>
#include <string.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <linux/vt.h>
#include <ctype.h>

struct auth *newAuthorizationInfo( void ) {
	struct auth *auth;

	auth = malloc( sizeof( struct auth ) );
	if ( !auth ) {
		LNCdebug( LNC_ERROR, 1, "newAuthorizationInfo() -> malloc()" );
		return NULL;
	}

	auth->username[ 0 ] = '\0';
	auth->password[ 0 ] = '\0';

	return auth;
}

int getDefaultAuthorizationInfo( struct auth*auth ) {
	int retval = -EXIT_FAILURE;


	lockLNCConfig( READ_LOCK );
	errno = ENOENT;
	if ( networkConfig.DefaultUsername ) {
		strlcpy( auth->username, networkConfig.DefaultUsername, NAME_MAX + 1 );

		if ( networkConfig.DefaultPassword )
			strlcpy( auth->password, networkConfig.DefaultPassword, NAME_MAX +1 );

		retval = errno = EXIT_SUCCESS;
	}
	unlockLNCConfig();

	return retval;
}

int readAuthorizationInfo( const char *URL, struct auth *auth ) {
	char key[ PATH_MAX ];
	char *ptr;

	if ( !auth || !URL ) {
		errno = EINVAL;
		return -EXIT_FAILURE;
	}

	auth->username[ 0 ] = auth->password[ 0 ] = '\0';
	lockLNCConfig( READ_LOCK );
	if ( networkConfig.DisablePasswordWriting ) {
		unlockLNCConfig();
		return EXIT_SUCCESS;
	}
	unlockLNCConfig();

	snprintf( key, PATH_MAX, "LNC/Authorization/%s", URL );
	initKeys( key );
	ptr = getCharKey( key, "username", NULL );
	if ( !ptr ) {
		cleanupKeys( key );
		return getDefaultAuthorizationInfo( auth );
	}

	strlcpy( auth->username, ptr, NAME_MAX + 1 );
	free( ptr );

	ptr = getCharKey( key, "password", NULL );
	if ( ptr ) {
		strlcpy( auth->password, ptr, NAME_MAX + 1 );
		free( ptr );
	}

	cleanupKeys( key );

	return EXIT_SUCCESS;
}

int writeAuthorizationInfo( const char *URL, struct auth *auth ) {
	int retval;
	char key[ PATH_MAX ];

	if ( !auth || !URL ) {
		errno = EINVAL;
		return -EXIT_FAILURE;
	}

	lockLNCConfig( READ_LOCK );
	if ( networkConfig.DisablePasswordWriting ) {
		unlockLNCConfig();
		return EXIT_SUCCESS;
	}
	unlockLNCConfig();

	snprintf( key, PATH_MAX, "LNC/Authorization/%s", URL );
	initKeys( key );
	retval = setCharKey( key, "username", auth->username, USER_ROOT );
	if ( retval < 0 ) {
		LNCdebug( LNC_ERROR, 1, "writeAuthorizationInfo( %s ) -> setCharKey( %s, username )", URL, key );
		cleanupKeys( key );
		return -EXIT_FAILURE;
	}

	retval = setCharKey( key, "password", auth->password, USER_ROOT );
	cleanupKeys( key );
	if ( retval < 0 ) {
		LNCdebug( LNC_ERROR, 1, "writeAuthorizationInfo( %s ) -> setCharKey( %s, password )", URL, key );
		return -EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

int requestAuthorizationDialog( const char *URL, struct auth *auth ) {
	LNCdebug( LNC_DEBUG, 0, "requestAuthorizationDialog() " );

	if ( LNCauthorizationCallback )
		return LNCauthorizationCallback( URL, auth );

	LNCdebug( LNC_DEBUG, 0, "requestAuthorizationDialog( %s ): No function defined" );
	errno = ENOENT;
	return -EXIT_FAILURE;
}

struct passwd *getSessionInfoByUID( uid_t sessionUID, char *buffer ) {
	struct passwd *entry = NULL, *ptr;
	size_t size = BUFSIZ;


	entry = ( struct passwd * ) buffer;
	getpwuid_r( sessionUID, entry, ( char * ) entry + sizeof( struct passwd ), size, &ptr );
	if ( !ptr && errno )
		LNCdebug( LNC_ERROR, 1, "getSessionInfoByUID(%d) -> getpwuid_r(%d)", sessionUID, sessionUID );
	else if ( !ptr )
		LNCdebug( LNC_ERROR, 0, "getSessionInfoByUID(%d) -> getpwuid_r(%d): No such user", sessionUID, sessionUID );

	return ptr;
}

struct passwd *getSessionInfoByName( char *userName, char *buffer ) {
	struct passwd *entry, *ptr;
	size_t size = BUFSIZ;

	entry = ( struct passwd * ) buffer;
	getpwnam_r( userName, entry, ( char * ) entry + sizeof( struct passwd ), size, &ptr );
	if ( !ptr && errno )
		LNCdebug( LNC_ERROR, 1, "getSessionInfoByName( %s ) -> getpwnam_r( %s )", userName, userName );
	else if ( !ptr )
		LNCdebug( LNC_ERROR, 0, "getSessionInfoByName( %s ) -> getpwnam_r( %s ): No such user", userName, userName );

	return ptr;
}

int getUIDbyPID( pid_t PID, uid_t *UID ) {
	char * file;
	int retval;
	char path[ PATH_MAX ];
	char *ptr;

	sprintf( path, "/proc/%d/status", PID );
	retval = copyFileToBuffer( &file, path );
	if ( retval < 0 ) {
		LNCdebug( LNC_ERROR, 1, "getUIDbyPID( %d ) -> copyFileToBuffer( %s )", PID, path );
		return -EXIT_FAILURE;
	}

	ptr = strstr( file, "Uid:" );
	if ( !ptr ) {
		LNCdebug( LNC_ERROR, 0, "getUIDbyPID( %d ) -> strstr( %s, Uid:): Invalid file format", PID, file );
		free( file );
		return -EXIT_FAILURE;
	}
	sscanf( ptr, "Uid:\t%d", ( int * ) UID );
	free( file );

	return EXIT_SUCCESS;
}

int getGIDbyPID( pid_t PID, gid_t *GID ) {
	char * file;
	int retval;
	char path[ PATH_MAX ];
	char *ptr;

	sprintf( path, "/proc/%d/status", PID );
	retval = copyFileToBuffer( &file, path );
	if ( retval < 0 ) {
		LNCdebug( LNC_ERROR, 1, "getGIDbyPID( %d ) -> copyFileToBuffer( %s )", PID, file );
		return -EXIT_FAILURE;
	}

	ptr = strstr( file, "Gid:" );
	if ( !ptr ) {
		LNCdebug( LNC_ERROR, 0, "getGIDbyPID( %d ) -> strstr( %s, Gid:): Invalid file format", PID, file );
		free( file );
		return -EXIT_FAILURE;
	}
	sscanf( ptr, "Gid:\t%d", ( int * ) GID );
	free( file );

	return EXIT_SUCCESS;
}

char *getTTYDeviceByPID( pid_t PID, char *buffer, size_t size ) {
	char path[ PATH_MAX ];
	int retval;

	sprintf( path, "/proc/%d/fd/1", PID );
	retval = readlink( path, buffer, size );
	if ( retval < 0 ) {
		LNCdebug( LNC_ERROR, 1, "getTTYDeviceByPID( %d ) ->readlink( %s )", PID, path );
		return NULL;
	}

	buffer[ retval ] = '\0';

	if ( ( strncmp( buffer, "/dev/tty", 8 ) == 0 ) ||
	        ( strncmp( buffer, "/dev/vc/", 8 ) == 0 ) ||
	        ( strncmp( buffer, "/dev/pts", 8 ) == 0 ) ) {
		return buffer;
	}

	return NULL;
}

int changeToUser( uid_t uid, gid_t gid ) {
	gid_t currentEGID;

	currentEGID = getegid();

	if ( setegid( gid ) < 0 ) {
		LNCdebug( LNC_ERROR, 1, "changeToUser() -> setegid(%d)", gid );
		return -EXIT_FAILURE;
	}

	if ( seteuid( uid ) < 0 ) {
		LNCdebug( LNC_ERROR, 1, "changeToUser() -> seteuid(%d)", uid );
		if ( setegid( currentEGID ) < 0 )
			LNCdebug( LNC_ERROR, 1, "Failed to restore previous Effective Groups ID" );
		return -EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
