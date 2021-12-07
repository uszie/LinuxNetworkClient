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

static const char FTPdescription[] = "This plugin allows you to browse networks that rely on the FTP protocol";
static const char FTPprotocol[] = "FTP";
static char FTPnetworkName[ NAME_MAX + 1 ];

static int FTPmount( Service *service, struct auth *auth ) {
	char mount_exec[ BUFSIZ ];
	char IPBuffer[ 16];
	char *IP;
	char *output;
	char path[ PATH_MAX ];
	int exitStatus;

	IP = IPtoString( service->IP, IPBuffer, 16 );
	if ( !IP )
		return -EINVAL;

	createServicePath( path, PATH_MAX, service );
	sprintf( mount_exec, "networkmount -t ftpfs \"%s\" \"%s\" -o user=%s:%s,uid=%d,gid=%d,allow_other", IP, path, ( auth && auth->username[ 0 ] != '\0' ) ? auth->username : "", ( auth && auth->password[ 0 ] != '\0' ) ? auth->password : "", geteuid(), getegid() );

	output = execute( mount_exec, STDOUT_FILENO | STDERR_FILENO, environ, &exitStatus );
	if ( !output )
		return -ENOMSG;

	if ( isMounted( path ) ) {
		free( output );
		return EXIT_SUCCESS;
	}

	LNCdebug( LNC_ERROR, 1,"FTPmount() -> isMounted( %s )", path );
	LNCdebug( LNC_ERROR, 0, "FTPmount() -> execute( %s ): %s", mount_exec, output );
	if ( strcasestr( output, "Access denied" ) ) {
		free( output );
		errno = EACCES;
		return -EACCES;
	} else if ( strcasestr( output, "couldn't connect to host" ) ) {
		free( output );
		errno = ENODEV;
		return -ENODEV;
	}

	free( output );
	errno = ENOMSG;
	return -ENOMSG;
}

static int FTPumount( Service *service ) {
	char * output;
	char umount_exec[ BUFSIZ ];
	char path[ PATH_MAX ];
	int exitStatus;

	if ( service->flags & LAZY_UMOUNT )
		LNCdebug( LNC_INFO, 0, "FTPumount(): forcing umount" );

	createServicePath( path, PATH_MAX, service );
	sprintf( umount_exec, "networkumount -t ftpfs %s\"%s\"", ( service->flags & LAZY_UMOUNT ) ? "-l " : "", path );
	output = execute( umount_exec, STDERR_FILENO | STDOUT_FILENO, environ, &exitStatus );
	if ( !output )
		return -ENOMSG;

	if ( ( exitStatus != 0 ) && ( exitStatus != 1 ) && ( exitStatus != 4 ) )
		LNCdebug( LNC_ERROR, 0, "FTPumount() output:\n%s", output );

	free( output );

	if ( isMounted( path ) )
		return -ENOMSG;

	return EXIT_SUCCESS;
}

static NetworkPlugin ftp =
{
	.minimumTouchDepth = 5,
 	.supportsAuthentication = 1,
 	.networkName = FTPnetworkName,
 	.description = FTPdescription,
 	.protocol = FTPprotocol,
 	.portNumber = 21,
 	.mount = FTPmount,
 	.umount = FTPumount,
};

void *giveNetworkPluginInfo( void ) {
	strlcpy( FTPnetworkName, gettext( "FTP Network" ), NAME_MAX + 1 );
	return &ftp;
}
