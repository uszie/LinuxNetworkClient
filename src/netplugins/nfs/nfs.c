/***************************************************************************
*   Copyright (C) 2003 by Usarin Heininga                                 *
*   usarin@usarin.heininga                                                *
*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License as published by  *
*   the Free Software Foundation; either version 2 of the License, or     *
*   (at your option) any later version.                                   *
***************************************************************************/

/*#include "../../optimized/config.h"*/
#include "networkclient.h"
#include "pluginhelper.h"

#include <rpcsvc/mount.h>
#include <netdb.h>
#include <rpc/rpc.h>
#include <rpc/pmap_clnt.h>
#include <rpcsvc/rstat.h>
#include <sys/time.h>
#include <signal.h>
#include <dirent.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <errno.h>
#include <unistd.h>
#include <libintl.h>

#define NFS_TIMEOUT 333333ULL
//volatile int NFSScanTimeout = 333333;

static const char NFSdescription[] = "This plugin allows you to browse networks that rely on the NFS protocol (Linux)";
static const char NFSprotocol[] = "NFS";
static char NFSnetworkName[ NAME_MAX + 1 ];

// static bool_t NFSreplyFunction( void *res, struct sockaddr_in *who );
// static void *NFSthread( void *arg );

static int NFSmount ( Service *service, struct auth *auth );
static int NFSumount ( Service *service );
// static int NFSgetDomains( Service *service );
// static int NFSgetHosts( Service *service );
static int NFSgetShares ( Service *service, struct auth *auth );


/*static pthread_key_t NFSthreadKey = 0;

static bool_t NFSreplyFunction( void *arg, struct sockaddr_in *who ) {
	struct hostent *hostent;
	char domain[ NAME_MAX + 1 ];
	char host[ NAME_MAX + 1 ];
	char *ptr;
	char URL[ PATH_MAX ];
	Service *new;
	Service *found;
	u_int64_t *NFSthreadStartTime;

	( void ) arg;

	NFSthreadStartTime = ( u_int64_t * ) createTSDBuffer( 0, &NFSthreadKey, 0 );
	if ( *NFSthreadStartTime + 4000000ULL < getCurrentTime() )
		return TRUE;

	hostent = gethostbyaddr( ( void * ) &who->sin_addr, sizeof( who->sin_addr ), AF_INET );
	if ( hostent ) {
		ptr = strchr( hostent->h_name, '.' );
		if ( ptr && *( ptr + 1 ) != '\0' ) {
			*ptr = '\0';
			strcpy( host, hostent->h_name );
			strcpy( domain, ptr + 1 );
		} else {
			if ( ptr )
				*ptr ='\0';
			strcpy( host, hostent->h_name );
			strcpy( domain, "(Unknown)" );
		}
	} else {
		strcpy( host, inet_ntoa( who->sin_addr ) );
		strcpy( domain, "(Unknown)" );
	}

	snprintf( URL, PATH_MAX, "network://%s/%s", NFSprotocol, domain );
	found = findListEntry( browselist, URL );
	if ( found ) {
		updateServiceTimeStamp( found );
		putListEntry( browselist, found );
	} else {
		new = newServiceEntry( 0, strdup( domain ), NULL, NULL, NULL, strdup( NFSprotocol ) );
		addListEntry( browselist, new );
	}

	snprintf( URL, PATH_MAX, "network://%s/%s/%s", NFSprotocol, domain, host );
	found = findListEntry( browselist, URL );
	if ( found ) {
		updateServiceTimeStamp( found );
		updateServiceIPAddress( found, ( unsigned long ) who->sin_addr.s_addr );
		putListEntry( browselist, found );
	} else {
		new = newServiceEntry( ( unsigned long ) who->sin_addr.s_addr, strdup( domain ), strdup( host ), NULL, NULL, strdup( NFSprotocol ) );
		addListEntry( browselist, new );
	}

	return FALSE;
}

static void *NFSthread( void *arg ) {
	enum clnt_stat clnt_stat;
	u_int64_t *NFSthreadStartTime;

	NFSthreadStartTime = ( u_int64_t * ) createTSDBuffer( sizeof( u_int64_t ), &NFSthreadKey, 1 );
	*NFSthreadStartTime = getCurrentTime();

	clnt_stat = clnt_broadcast ( 100005, 3, NULLPROC, ( xdrproc_t ) xdr_void, NULL, ( xdrproc_t ) xdr_void, arg, ( resultproc_t ) NFSreplyFunction );
	if ( clnt_stat != RPC_SUCCESS && clnt_stat != RPC_TIMEDOUT ) {
		LNCdebug( LNC_ERROR, 0, "nfsThread() -> clnt_broadcast(): Failed to get hosts" );
		return ( void * ) - ENOMSG;
	}

	return ( void * ) EXIT_SUCCESS;
}*/

static int NFSmount ( Service *service, struct auth *auth )
{
	char mount_exec[ BUFSIZ ];
	char *output;
	char share[ PATH_MAX ];
	char *ptr;
	char path[ PATH_MAX ];
	char IPBuffer[ 16 ];
	char *IP;
	int exitStatus;
	int retry = 1;

	( void ) auth;

	strlcpy ( share, service->share, PATH_MAX );
	ptr = share;
	while ( ( ptr = strchr ( ptr, '|' ) ) )
		* ptr = '/';

	IP = IPtoString( service->IP, IPBuffer, 16 );
	if ( !IP )
		return -EINVAL;

	createServicePath ( path, PATH_MAX, service );
	sprintf ( mount_exec, "networkmount -t nfs \"%s:%s\" \"%s\" -o soft,rw", IP, share, path );

retry:
	output = execute ( mount_exec, STDOUT_FILENO | STDERR_FILENO, environ, &exitStatus );
	if ( !output )
		return -ENOMSG;

	if ( isMounted ( path ) )
	{
		free ( output );
		return EXIT_SUCCESS;
	}

	if ( strlen ( output ) > 1 )
	{
		LNCdebug ( LNC_ERROR, 0, "NFSMount() -> execute(): Failed to mount service" );
		LNCdebug ( LNC_ERROR, 0, "NFSMount() ->execute(): %s", mount_exec, output );
	}

	free ( output );

	if ( retry )
	{
		LNCdebug ( LNC_INFO, 0, "NFSmount(): failed to mount read/write, try'ing read only" );
		sprintf ( mount_exec, "networkmount -t nfs \"%s:%s\" \"%s\" -o soft,ro", IPtoString ( service->IP, IPBuffer, 16 ), share, path );
		retry = 0;
		goto retry;
	}

	return -ENOMSG;
}

static int NFSumount ( Service *service )
{
	char * output;
	char umount_exec[ BUFSIZ ];
	char path[ PATH_MAX ];
	int exitStatus;

	if ( service->flags & LAZY_UMOUNT )
		LNCdebug ( LNC_INFO, 0, "NFSumount(): forcing umount" );

	createServicePath ( path, PATH_MAX, service );
	sprintf ( umount_exec, "networkumount -t nfs %s\"%s\"", ( service->flags & LAZY_UMOUNT ) ? "-l " : "", path );
	output = execute ( umount_exec, STDERR_FILENO | STDOUT_FILENO, environ, &exitStatus );
	if ( !output )
		return -ENOMSG;

	if ( ( exitStatus != 0 ) && ( exitStatus != 1 ) && ( exitStatus != 4 ) )
		LNCdebug ( LNC_ERROR, 0, "NFSumount() output:\n%s", output );

	free ( output );

	if ( isMounted ( path ) )
		return -ENOMSG;

	return EXIT_SUCCESS;
}

// static int NFSgetDomains( Service *service ) {
// 	void * retval;
//
// 	( void ) service;
//
// 	retval = createThread( NFSthread, NULL, DETACHED );
// 	usleep( NFS_TIMEOUT );
//
// 	return EXIT_SUCCESS;
// }
//
// static int NFSgetHosts( Service *service ) {
// 	void * retval;
//
// 	retval = createThread( NFSthread, service->domain, DETACHED );
// 	usleep( NFS_TIMEOUT );
//
// 	return EXIT_SUCCESS;
// }

static int NFSgetShares ( Service *service, struct auth *auth )
{
	enum clnt_stat clnt_stat;
	struct sockaddr_in server_addr;
	struct timeval total_timeout;
	struct timeval pertry_timeout;
	CLIENT *mclient;
	int msock, cnt;
	int i;
	exports exportlist;
	exports testptr;
	char **shares, *ptr;
	char URL[ PATH_MAX ];
	char IPBuffer[ 16 ];
	Service *new;
	Service *found;

	( void ) auth;

	inet_aton ( IPtoString ( service->IP, IPBuffer, 16 ), &server_addr.sin_addr );
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = 0;
	msock = RPC_ANYSOCK;

	mclient = clnttcp_create ( &server_addr, MOUNTPROG, MOUNTVERS, &msock, 0, 0 );
	if ( !mclient )
	{
		server_addr.sin_port = 0;
		msock = RPC_ANYSOCK;
		pertry_timeout.tv_sec = NFS_TIMEOUT / 1000000ULL;
		pertry_timeout.tv_usec = NFS_TIMEOUT - ( pertry_timeout.tv_sec * 1000000ULL );
		mclient = clntudp_create ( &server_addr, MOUNTPROG, MOUNTVERS, pertry_timeout, &msock );
		if ( !mclient )
		{
			LNCdebug ( LNC_ERROR, 0, "NFSgetShares() -> clntudp_create(): Failed to create client connection to network://%s/%s on %s", service->network, service->host, IPtoString ( service->IP, IPBuffer, 16 ) );
			errno = ENOMSG;
			return -ENOMSG;
		}
	}

	mclient->cl_auth = authunix_create_default();
	total_timeout.tv_sec = 20;
	total_timeout.tv_usec = 0;

	memset ( &exportlist, '\0', sizeof ( exportlist ) );
	clnt_stat = clnt_call ( mclient, MOUNTPROC_EXPORT, ( xdrproc_t ) xdr_void, NULL, ( xdrproc_t ) xdr_exports, ( caddr_t ) & exportlist, total_timeout );
	auth_destroy ( mclient->cl_auth );
	clnt_destroy ( mclient );
	if ( clnt_stat != RPC_SUCCESS )
	{
		LNCdebug ( LNC_ERROR, 0, "NFSgetShares() -> clnt_call(): Failed to retreive shares list from network://%s/%s on %s", service->network, service->host, IPtoString ( service->IP, IPBuffer, 16 ) );
		errno = ENOMSG;
		return -ENOMSG;
	}

	testptr = exportlist;
	shares = malloc ( SHARES_MAX * sizeof ( char * ) );
	for ( cnt = 0; exportlist; exportlist = exportlist->ex_next )
	{
		ptr = shares[ cnt++ ] = strdup ( exportlist->ex_dir );
		while ( ( ptr = strchr ( ptr, '/' ) ) )
			* ptr = '|';
	}

	if ( !shares[ 0 ] )
	{
		free ( shares );
		return EXIT_SUCCESS;
	}
	else
		shares[ cnt ] = NULL;

	exportlist = testptr;
	while ( exportlist )
	{
		free ( exportlist->ex_dir );
		free ( exportlist->ex_groups->gr_name );
		free ( exportlist->ex_groups );
		testptr = exportlist->ex_next;
		free ( exportlist );
		exportlist = testptr;
	}

	for ( i = 0; shares[ i ]; ++i )
	{
		if ( service->domain )
			snprintf ( URL, PATH_MAX, "network://%s/%s/%s/%s", NFSprotocol, service->domain, service->host, shares[ i ] );
		else
			snprintf ( URL, PATH_MAX, "network://%s/%s/%s", NFSprotocol, service->host, shares[ i ] );
		lockList ( browselist, WRITE_LOCK );
		found = findListEntryUnlocked ( browselist, URL );
		if ( !found )
		{
			new = newServiceEntry ( service->IP, service->domain ? strdup ( service->domain ) : NULL, strdup ( service->host ), strdup ( shares[ i ] ), NULL, strdup ( NFSprotocol ) );
			addListEntryUnlocked ( browselist, new );
		}
		else
		{
			updateServiceTimeStamp ( found );
			putListEntry ( browselist, found );
		}
		unlockList ( browselist );
		free ( shares[ i ] );
	}

	free ( shares );

	return EXIT_SUCCESS;
}

static int NFSinit ( void )
{
	return EXIT_SUCCESS;
}

static void NFScleanUp ( void )
{
}

static NetworkPlugin nfs =
{
	.minimumTouchDepth = 6,
	.supportsAuthentication = 0,
	.init = NFSinit,
	.cleanUp = NFScleanUp,
	.networkName = NFSnetworkName,
	.description = NFSdescription,
	.protocol = NFSprotocol,
	.portNumber = 2049,
	.mount = NFSmount,
	.umount = NFSumount,
//	.getDomains = NFSgetDomains,
//	.getHosts = NFSgetHosts,
	.getShares = NFSgetShares,
};

void *giveNetworkPluginInfo ( void )
{
	strlcpy ( NFSnetworkName, gettext ( "Linux Network" ), NAME_MAX + 1 );
	return & nfs;
}
