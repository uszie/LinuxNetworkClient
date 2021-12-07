/***************************************************************************
*   Copyright (C) 2003 by Usarin Heininga                                 *
*   usarin@heininga.nl                                                    *
*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License as published by  *
*   the Free Software Foundation; either version 2 of the License, or     *
*   (at your option) any later version.                                   *
***************************************************************************/

#include"config.h"
#include "session.h"
#include "daemon.h"
#include "fusefs.h"
#include "dir.h"
#include "nconfig.h"
#include "plugin.h"

#include <stdlib.h>
#include <pwd.h>
#include <sys/types.h>
#include <time.h>
#include <utmp.h>
#include <signal.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/mount.h>
#include <sys/wait.h>
#include <mntent.h>
#include <grp.h>
#include <sys/mman.h>
#include <libintl.h>

struct sessionList __sessionList;
struct sessionList *sessionList = &__sessionList;
char *networkDirectory;

int spawnSession( Session *session );
int updateSessions( const char *path, const char *filename, int event );
Session *sessionNew( uid_t sessionUID, gid_t sessionGID, char *userName, char *homeDirectory );
static void sessionFree( void *data );
static void sessiononInsert( Session *session );
static void sessiononRemove( Session *session );
static int sessionCompare( void *entry1, void *entry2 );
static int sessionFind( uid_t *search, void *entry );
static void sessionPrint( void *data ) ;
void createSessionList( void );

int spawnSession( Session *session ) {
	setsid();
	chdir( session->homeDirectory );
	setenv( "HOME", session->homeDirectory, 1 );
	setenv( "USER", session->userName, 1 );
	LNCdebug( LNC_INFO, 0, "Spawning new session for user %s:%d:%d, homedirectory %s", session ->userName, session->sessionUID, session->sessionGID, session->homeDirectory );

	LNCauthorizationCallback = networkclientCallback;
	LNCinit( 0, NULL );
	readNetworkClientConfiguration();

	networkDirectory = gettext( "Local Network" );
	sprintf( mountPath, "%s/Desktop/%s", session->homeDirectory, networkDirectory );
	mountPathLength = strlen( mountPath );

	lockLNCConfig( READ_LOCK );
	if ( !networkConfig.EnableDebugMode ) {
		sprintf( registryPath, "%s/.kdb/user/sw/networkclient/Applications", session->homeDirectory );
		addFileToMonitorList( registryPath, updateNetworkClientConfiguration );
	}
	unlockLNCConfig();

	sessionFree( session );
	sigprocmask( SIG_UNBLOCK, &blockMask, NULL );
	fuseMount( mountPath );

	cleanUp();

	return EXIT_SUCCESS;
}

int updateSessions( const char *path, const char *filename, int event ) {
	struct utmp user, *ptr;
	struct passwd *pw;
	Session *session, *found, *entry;
	static u_int64_t lastTime = 0ULL;
	u_int64_t currentTime;
	char buffer[ BUFSIZ ];

	( void ) path;
	( void ) filename;

	if ( event != FAMChanged )
		return EXIT_SUCCESS;

	currentTime = getCurrentTime();
	if ( ( lastTime + 1000000ULL ) > currentTime )
		return EXIT_SUCCESS;
	lastTime = currentTime;

	setutent();
	while ( getutent_r( &user, &ptr ) == 0 ) {
		LNCdebug( LNC_INFO, 0, "updateSessions(): found user %s type %d", user.ut_user, user.ut_type );
		if ( ( user.ut_type == USER_PROCESS ) && ( user.ut_user[ 0 ] != '\0' ) ) {
			pw = getSessionInfoByName( user.ut_user, buffer );
			if ( !pw )
				continue;
			found = findListEntry( sessionList, &pw->pw_uid );
			if ( found ) {
				found->timeStamp = currentTime;
				putListEntry( sessionList, found );
			} else {
				session = sessionNew( pw->pw_uid, pw->pw_gid, pw->pw_name, pw->pw_dir );
				session->timeStamp = currentTime;
				addListEntry( sessionList, session );
			}
		} else
			LNCdebug( LNC_INFO, 0, "updateSessions(): skipping user %s type %d", user.ut_user, user.ut_type );
	}
	endutent();

	lockList( sessionList, WRITE_LOCK );
	forEachListEntry( sessionList, entry ) {
		if ( entry->timeStamp < currentTime )
			delListEntryUnlocked( sessionList, entry );
	}
	unlockList( sessionList );

	LNCdebug( LNC_INFO, 0, "updateSessions(): Current session list" );
	printList( sessionList );
	LNCdebug( LNC_INFO, 0, "" );
	return 0;
}

Session *sessionNew( uid_t sessionUID, gid_t sessionGID, char *userName, char *homeDirectory ) {
	Session * new;

	new = malloc( sizeof( Session ) );
	if ( !new )
		return NULL;

	new->sessionUID = sessionUID;
	new->sessionGID = sessionGID;
	new->userName = strdup( userName );
	new->homeDirectory = strdup( homeDirectory );

	return new;
}

static void sessionFree( void *data ) {
	Session * session = ( Session * ) data;

	free( session->userName );
	free( session->homeDirectory );
	free( session );
}

static void sessiononInsert( Session *session ) {
	pid_t pid;

	pid = fork();
	if ( pid < 0 )
		LNCdebug( LNC_ERROR, 1, "sessiononInsert( %s ) -> fork()", session->userName );
	else if ( pid == 0 ) {
		char argument[ PATH_MAX ];

		setresgid( session->sessionGID, session->sessionGID, session->sessionGID );
		setresuid( session->sessionUID, session->sessionUID, session->sessionUID );
		setenv( "USER", session->userName, 1 );
		setenv( "HOME", session->homeDirectory, 1 );
		snprintf( argument, PATH_MAX, "networkclient --user %s", session->userName );
		execlp( "bash", "bash", "-l", "-c", argument, NULL );
		LNCdebug( LNC_ERROR, 1, "sessiononInsert( %s ) -> execlp()", session->userName );
		_exit( 255 );
	}

	session->sessionPID = pid;
}

static void sessiononRemove( Session *session ) {
	int status;

	LNCdebug( LNC_INFO, 0, "sessiononRemove(): terminating session %d for user %s ( %d:%d )", session->sessionPID, session->userName, session->sessionUID, session->sessionGID );
	status = kill( session->sessionPID, SIGTERM );
	if ( status < 0 ) {
		if ( errno == ESRCH )
			LNCdebug( LNC_INFO, 0, "sessiononRemove( %s ) ->kill( %d, SIGTERM ): Session already removed", session->userName, session->sessionPID );
		else
			LNCdebug( LNC_ERROR, 1, "sessiononRemove( %s ) ->kill( %d, SIGTERM )", session->userName, session->sessionPID );
	} else
		waitpid( session->sessionPID, &status, 0 );
}

static int sessionCompare( void *entry1, void *entry2 ) {
	uid_t UID1 = ( ( Session * ) entry1 ) ->sessionUID;
	uid_t UID2 = ( ( Session * ) entry2 ) ->sessionUID;

	if ( UID1 == UID2 )
		return 0;
	else if ( UID1 < UID2 )
		return -1;
	else
		return 1;
}

static int sessionFind( uid_t *search, void *entry ) {
	uid_t UID1 = *search;
	uid_t UID2 = ( ( Session * ) entry ) ->sessionUID;

	if ( UID1 == UID2 )
		return 0;
	else if ( UID1 < UID2 )
		return -1;
	else
		return 1;
}

static void sessionPrint( void *data ) {
	Session * session = ( Session * ) data;

	LNCdebug( LNC_ERROR, 0, "Username = %s, Home directory = %s", session->userName, session->homeDirectory );
}

void createSessionList( void ) {
	sessionList = initList( sessionList, sessionCompare, sessionFind, sessionFree, sessionPrint, sessiononInsert, NULL, sessiononRemove );
}
