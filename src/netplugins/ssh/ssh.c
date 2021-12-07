/***************************************************************************
*   Copyright (C) 2003 by Usarin Heininga                                 *
*   usarin@heininga.nl                                                    *
*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License as published by  *
*   the Free Software Foundation; either version 2 of the License, or     *
*   (at your option) any later version.                                   *
***************************************************************************/


#include "networkclient.h"
#include "list.h"
#include "nconfig.h"
#include "plugin.h"
#include "pluginhelper.h"
#include "user.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/types.h>
#include <signal.h>
#include <libintl.h>
#include <pty.h>
#include <wait.h>

static const char SSHdescription[] = "This plugin allows you to browse networks that rely on the SSH protocol (Secure Shell)";
static const char SSHprotocol[] = "SSH";
static char SSHnetworkName[ NAME_MAX + 1 ];

static int useHostFile = 0;

static int SSHmount( Service *service, struct auth *auth ) {
	char mount_exec[ BUFSIZ ];
	char IPBuffer[ 16 ];
	char path[ PATH_MAX ];
	char hostKeyArgs[ PATH_MAX ];
	char *IP;
	char *output;
	char *ptr;
	int exitStatus;

	IP = IPtoString( service->IP, IPBuffer, 16 );
	if ( !IP )
		return -EINVAL;

	if ( useHostFile )
		hostKeyArgs[ 0 ] = '\0';
	else
		snprintf( hostKeyArgs, PATH_MAX, ",StrictHostKeyChecking=no,UserKnownHostsFile=/dev/null" );

	createServicePath( path, PATH_MAX, service );
	sprintf( mount_exec, "networkmount -t sshfs \"%s:/\" \"%s\" -o username=%s,password=%s,uid=%d,gid=%d,allow_other%s", IP, path, ( auth && auth->username[ 0 ] != '\0' ) ? auth->username : "", ( auth && auth->password[ 0 ] != '\0' ) ? auth->password : "", geteuid(), getegid(), hostKeyArgs );

	output = execute( mount_exec, STDOUT_FILENO | STDERR_FILENO, environ, &exitStatus );
	if ( !output )
		return -ENOMSG;

	if ( isMounted( path ) ) {
		free( output );
		return EXIT_SUCCESS;
	}

	LNCdebug( LNC_ERROR, 1,"SSHmount() -> isMounted( %s )", path );
	LNCdebug( LNC_ERROR, 0, "SSHmount() -> execute( %s ): %s", mount_exec, output );
	if ( strcasestr( output, "Permission denied, please try again." ) ) {
		free( output );
		errno = EACCES;
		return -EACCES;
	}

	ptr = strcasestr( output, "Password:" );
	if ( ptr && strcasestr( ptr+9, "Password" ) ) {
		free( output );
		errno = EACCES;
		return -EACCES;
	}

	free( output );
	errno = ENOMSG;
	return -ENOMSG;
}

static int SSHumount( Service *service ) {
	char * output;
	char umount_exec[ BUFSIZ ];
	char path[ PATH_MAX ];
	int exitStatus;

	if ( service->flags & LAZY_UMOUNT )
		LNCdebug( LNC_INFO, 0, "SSHumount(): forcing umount" );

	createServicePath( path, PATH_MAX, service );
	sprintf( umount_exec, "networkumount -t sshfs %s\"%s\"", ( service->flags & LAZY_UMOUNT ) ? "-l " : "", path );
	output = execute( umount_exec, STDERR_FILENO | STDOUT_FILENO, environ, &exitStatus );
	if ( !output )
		return -ENOMSG;

	if ( ( exitStatus != 0 ) && ( exitStatus != 1 ) && ( exitStatus != 4 ) )
		LNCdebug( LNC_ERROR, 0, "SSHumount() output:\n%s", output );

	free( output );

	if ( isMounted( path ) )
		return -ENOMSG;

	return EXIT_SUCCESS;
}

static int SSHmonitorConfiguration ( const char *path, const char *filename, int event )
{
	char *configEntry;

	LNCdebug ( LNC_DEBUG, 0, "SSHmonitorConfiguration(): path = %s, filename = %s, event = %d", path, filename, event );

	initKeys ( "LNC/plugins/SSH" );
	if ( event == FAMCreated || event == FAMChanged || event == FAMDeleted )
	{
		configEntry= getCharKey ( "LNC/plugins/SSH", "UseHostFile", "False" );
		if ( strcasecmp ( configEntry, "True" ) == 0 )
			useHostFile = 1;
		else
			useHostFile = 0;

		LNCdebug ( LNC_INFO, 0, "SSHmonitorConfiguration(): UseHostFile = %d", useHostFile );
		free ( configEntry );
	}

	cleanupKeys ( "LNC/plugins/SSH" );

	return EXIT_SUCCESS;
}

static  int SSHinit ( void )
{
	char *configEntry;
	char buffer[ BUFSIZ ];
	char path[ PATH_MAX ];
	struct passwd * user;

	initKeys ( "LNC/plugins/SSH" );
	configEntry= getCharKey ( "LNC/plugins/SSH", "UseHostFile", "False" );
	if ( strcasecmp ( configEntry, "True" ) == 0 )
		useHostFile = 1;
	else
		useHostFile = 0;

	LNCdebug ( LNC_INFO, 0, "SSHmonitorConfiguration(): UseHostFile = %d", useHostFile );
	free ( configEntry );

	setCharKey ( "LNC/plugins/SSH", "UseHostFile", useHostFile ? "True" : "False", USER_ROOT );
	cleanupKeys ( "LNC/plugins/SSH" );

	user = getSessionInfoByUID ( getuid(), buffer );
	if ( !user )
	{
		LNCdebug ( LNC_ERROR, 1, "SSHinit() -> getSessionInfoByUID( %d )", getuid() );
		return -EXIT_FAILURE;
	}

	snprintf ( path, PATH_MAX, "%s/%s/user/sw/LNC/plugins/SSH", user->pw_dir, KDB_DB_USER );
	addFileToMonitorList ( path, SSHmonitorConfiguration );

	return 0;
}

static  void SSHcleanUp ( void )
{
	char buffer[ BUFSIZ ];
	char path[ PATH_MAX ];
	struct passwd * user;

	user = getSessionInfoByUID ( getuid(), buffer );
	if ( !user )
	{
		LNCdebug ( LNC_ERROR, 1, "SSHcleanUp(() -> getSessionInfoByUID( %d )", getuid() );
		return;
	}

	snprintf ( path, PATH_MAX, "%s/%s/user/sw/LNC/plugins/SSH", user->pw_dir, KDB_DB_USER );
	delFileFromMonitorList ( path, SSHmonitorConfiguration );
}


static NetworkPlugin ssh =
    {
        .minimumTouchDepth = 5,
        .supportsAuthentication = 1,
	.init = SSHinit,
 	.cleanUp = SSHcleanUp,
        .networkName = SSHnetworkName,
        .description = SSHdescription,
        .protocol = SSHprotocol,
	.portNumber = 22,
        .mount = SSHmount,
        .umount = SSHumount,
    };

void *giveNetworkPluginInfo( void ) {
	strlcpy( SSHnetworkName, gettext( "Secure Shell Network" ), NAME_MAX + 1 );
	return & ssh;
}
