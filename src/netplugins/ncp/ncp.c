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

#include <ncp/ncplib.h>
#include <ncp/nwcalls.h>
#include <ncp/nwnet.h>

static const char NCPdescription[] = "This plugin allows you to browse networks that rely on the NCP protocol (Novell Netware)";
static const char NCPprotocol[] = "NCP";
static char NCPnetworkName[ NAME_MAX + 1 ];

static  int NCPerror ( char *input )
{
	errno = 0;

	if ( strstr ( input, ": Unknown user" ) )
		errno = EACCES;
	else if ( strstr ( input, "Login denied" ) )
		errno = EACCES;
	else if ( strstr ( input, ": User name is not specified" ) )
		errno = EACCES;
	else if ( strstr ( input, ": Server not found" ) )
		errno = ENODEV;
	else if ( strstr ( input, "mount.ncp" ) )
		errno = ENOMSG;
	return errno;
}

static  int NCPmount ( Service *service, struct auth *auth )
{
	int error;
	int exitStatus;
	char mount_exec[ BUFSIZ ];
	char path[ PATH_MAX ];
	char *environment[] = { "USER=", NULL };
	char IPBuffer[ 16 ];
	char *IP;
	char *output;

	IP = IPtoString( service->IP, IPBuffer, 16 );
	if ( !IP )
		return -EINVAL;

	createServicePath ( path, PATH_MAX, service );
	sprintf ( mount_exec, "networkmount -t ncpfs \"%s\" \"%s\" -o volume=%s,U=%s,P=%s,d=755,f=644,u=%d,g=%d,m", IP, path, service->share, ( auth && auth->username ) ? auth->username : "", ( auth && auth->password ) ? auth->password : "", geteuid(), getegid() );

	output = execute ( mount_exec, STDOUT_FILENO | STDERR_FILENO, environment, &exitStatus );
	if ( !output )
		return -ENOMSG;

	if ( isMounted ( path ) )
	{
		free ( output );
		return EXIT_SUCCESS;
	}

	LNCdebug ( LNC_ERROR, 1, "NCPmount() -> isMounted( %s )", path );
	LNCdebug ( LNC_ERROR, 0, "NCPmount() -> execute( %s ): %s", mount_exec, output );
	error = NCPerror ( output );
	free ( output );
	errno = error ? error : ENOMSG;

	return -errno;
}

static  int NCPumount ( Service *service )
{
	char * output;
	char umount_exec[ BUFSIZ ];
	char path[ PATH_MAX ];
	int exitStatus;

	if ( service->flags & LAZY_UMOUNT )
		LNCdebug ( LNC_INFO, 0, "NCPumount(): forcing umount" );

	createServicePath ( path, PATH_MAX, service );
	sprintf ( umount_exec, "networkumount -t ncpfs %s\"%s\"", ( service->flags & LAZY_UMOUNT ) ? "-l " : "", path );
	output = execute ( umount_exec, STDERR_FILENO | STDOUT_FILENO, environ, &exitStatus );
	if ( !output )
		return -ENOMSG;

	if ( ( exitStatus != 0 ) && ( exitStatus != 1 ) && ( exitStatus != 4 ) )
		LNCdebug ( LNC_ERROR, 0, "NCPumount() output:\n%s", output );

	free ( output );

	if ( isMounted ( path ) )
		return -ENOMSG;

	return EXIT_SUCCESS;
}

static  int NCPgetDomains ( Service *service )
{
	Service *new;
	Service *found;
	struct ncp_conn *connection;
	struct ncp_bindery_object obj;
	long error;
	char *domain;
	char URL[ PATH_MAX ];

	( void ) service;

	connection = ncp_open ( NULL, &error ) ;
	if ( connection == NULL )
	{
		LNCdebug ( LNC_ERROR, 0, "NCPgetDomains() -> ncp_open(): %s", strnwerror ( error ) );
		return -ENODEV;
	}

	obj.object_id = 0xffffffff;

	while ( ncp_scan_bindery_object ( connection, obj.object_id, NCP_BINDERY_FSERVER, "*", &obj ) == 0 )
	{
		domain = strchr ( ( char * ) obj.object_name, '.' );
		if ( !domain )
			domain = "(Unknown)";
		else
			++domain;
		snprintf ( URL, PATH_MAX, "network://%s/%s", NCPprotocol, domain );
		found = findListEntry ( browselist, URL );
		if ( found )
		{
			updateServiceTimeStamp ( found );
			putListEntry ( browselist, found );
		}
		else
		{
			new = newServiceEntry ( 0, strdup ( domain ), NULL, NULL, NULL, strdup ( NCPprotocol ) );
			addListEntry ( browselist, new );
		}
	}

	NWCCCloseConn ( connection );

	return EXIT_SUCCESS;
}

static  int NCPgetHosts ( Service *service )
{
	Service * new;
	Service *found;
	struct ncp_conn *connection;
	struct ncp_bindery_object obj;
	long error;
	char *domain;
	char *host;
	char URL[ PATH_MAX ];
	char fullHostName[ NCP_BINDERY_NAME_LEN ];

	connection = ncp_open ( NULL, &error ) ;
	if ( connection == NULL )
	{
		LNCdebug ( LNC_ERROR, 0, "NCPgetHosts() -> ncp_open(): %s", strnwerror ( error ) );
		return -ENODEV;
	}

	obj.object_id = 0xffffffff;

	while ( ncp_scan_bindery_object ( connection, obj.object_id, NCP_BINDERY_FSERVER, "*", &obj ) == 0 )
	{
		strlcpy ( fullHostName, ( char * ) obj.object_name, NCP_BINDERY_NAME_LEN );
		domain = strchr ( ( char * ) obj.object_name, '.' );
		if ( !domain )
		{
			domain = "(Unknown)";
			host = ( char * ) obj.object_name;
		}
		else
		{
			*domain = '\0';
			++domain;
			host = ( char * ) obj.object_name;
		}

		if ( strcmp ( domain, service->domain ) == 0 )
		{
			snprintf ( URL, PATH_MAX, "network://%s/%s/%s", NCPprotocol, service->domain, host );
			found = findListEntry ( browselist, URL );
			if ( found )
			{
				updateServiceTimeStamp ( found );
				putListEntry ( browselist, found );
			}
			else
			{
				new = newServiceEntry ( getIPAddress ( fullHostName ), strdup ( service->domain ), strdup ( host ), NULL, NULL, strdup ( NCPprotocol ) );
				addListEntry ( browselist, new );
			}
		}
	}

	NWCCCloseConn ( connection );

	return EXIT_SUCCESS;
}

static  int NCPgetShares ( Service *service, struct auth *auth )
{
	Service *new;
	Service *found;
	struct ncp_conn *connection;
	NWDSCCODE error;
	char URL[ PATH_MAX ];
	char server[ PATH_MAX ];
	unsigned int v;
	char vName[ 256 ];
	u_int64_t destns = NW_NS_DOS;
	NWVOL_HANDLE handle;

	( void ) auth;

	NWCallsInit ( NULL, NULL );
	if ( strcmp ( service->domain, "(Unknown)" ) == 0 )
		strlcpy ( server, service->host, PATH_MAX );
	else
		sprintf ( server, "%s.%s", service->host, service->domain );

	error = NWCCOpenConnByName ( NULL, server, NWCC_NAME_FORMAT_BIND, 0, NWCC_RESERVED, &connection );
	if ( error )
	{
		LNCdebug ( LNC_ERROR, 0, "NCPgetShares() -> NWCCOpenConnByName( %s ): %s", server, strnwerror ( error ) );
		return -ENODEV;
	}

	error = ncp_volume_list_init ( connection, destns, 1, &handle );
	if ( error )
	{
		NWCCCloseConn ( connection );
		LNCdebug ( LNC_ERROR, 0, "NCPgetShares() -> ncp_volume_list_init(): %s", strnwerror ( error ) );
		return -ENODEV;
	}

	while ( ( error = ncp_volume_list_next ( handle, &v, vName, sizeof ( vName ) ) ) == 0 )
	{
		snprintf ( URL, PATH_MAX, "network://%s/%s/%s", NCPprotocol, service->host, vName );
		lockList ( browselist, WRITE_LOCK );
		found = findListEntryUnlocked ( browselist, URL );
		if ( !found )
		{
			new = newServiceEntry ( service->IP, strdup ( service->domain ), strdup ( service->host ), strdup ( vName ), NULL, strdup ( NCPprotocol ) );
			addListEntryUnlocked ( browselist, new );
		}
		else
		{
			updateServiceTimeStamp ( found );
			putListEntry ( browselist, found );
		}
		unlockList ( browselist );
	}

	ncp_volume_list_end ( handle );
	NWCCCloseConn ( connection );

	return EXIT_SUCCESS;
}

static  int NCPinit ( void )
{
	return EXIT_SUCCESS;
}

static  void NCPcleanUp ( void )
{
}

static NetworkPlugin ncp =
{
	.minimumTouchDepth = 6,
	.supportsAuthentication = 1,
	.init = NCPinit,
	.cleanUp = NCPcleanUp,
	.networkName = NCPnetworkName,
	.description = NCPdescription,
	.protocol = NCPprotocol,
	.mount = NCPmount,
	.umount = NCPumount,
	.getDomains = NCPgetDomains,
	.getHosts = NCPgetHosts,
	.getShares = NCPgetShares,
};

void *giveNetworkPluginInfo ( void )
{
	strlcpy ( NCPnetworkName, gettext ( "Novell Network" ), NAME_MAX + 1 );
	return & ncp;
}
