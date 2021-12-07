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

#include <kdb.h>
#include <kdbprivate.h>

static const char SMBdescription[] = "This plugin allows you to browse networks that rely on the SMB protocol (Windows)";
static const char SMBprotocol[] = "SMB";
static char SMBnetworkName[ NAME_MAX + 1 ];

static int useCifs = 1;
static int smbTTL = 1000;

static  int SMBerror ( char *input )
{
	errno = 0;

	if ( strstr ( input, "mount error 13 " ) )
		errno = EACCES;
	else if ( strstr ( input, "mount error(13)" ) )
		errno = EACCES;
	else if ( strcasestr ( input, "Permission denied" ) )
		errno = EACCES;
	else if ( strstr ( input, "mount error 1 " ) )
		errno = EACCES;
	else if ( strstr ( input, "mount error 20" ) )
		errno = ENOMEDIUM;
	else if ( strstr ( input, "NT_STATUS_ACCESS_DENIED" ) )
		errno = EACCES;
	else if ( strstr ( input, "ERRSRV - ERRbadpw" ) )
		errno = EACCES;
	else if ( strstr ( input, "ERRDOS" ) &&
	          strstr ( input, "ERRnoaccess" ) )
		errno = EACCES;
	else if ( strstr ( input, "Operation not permitted" ) )
		errno = EACCES;
	else if ( strstr ( input, "mount error 6 " ) )
		errno = ENODEV;
	else if ( strstr ( input, "could not find target server" ) )
		errno = ENODEV;
	else if ( strstr ( input, "Connection to" ) &&
	          strstr ( input, "failed" ) )
		errno = ENODEV;
	else if ( strstr ( input, "Connection to" ) &&
	          strstr ( input, "failed" ) )
		errno = ENODEV;
	else if ( strstr ( input, "ERRDOS" ) &&
	          ( strstr ( input, "ERRbadfile" ) ||
	            strstr ( input, "ERRnosuchshare" ) ) )
		errno = ENODEV;
	else if ( strstr ( input, "NT_STATUS_BAD_NETWORK_NAME" ) )
		errno = ENODEV;
	else if ( strstr ( input, "Broken pipe" ) )
		errno = ENOMSG;
	else if ( strstr ( input, "mount error" ) )
		errno = ENOMSG;
	else if ( strstr ( input, "Usage" ) )
		errno = ENOMSG;
	else if ( strstr ( input, "ERRDOS - ERRnomem" ) )
		errno = ENOMSG;
	else if ( strstr ( input, "smbclient not found" ) )
		errno = ENOMSG;

	return errno;
}

static  int SMBmount ( Service *service, struct auth *auth )
{
	int error;
	char mount_exec[ BUFSIZ ];
	char *output;
	char path[ PATH_MAX ];
	int exitStatus;
	char authString[ BUFSIZ ];
	char IPBuffer[ 16 ];
	//	char *environment[] = { "USER=", NULL };

	createServicePath ( path, PATH_MAX, service );
	if ( useCifs )
	{
		if ( auth && auth->username[ 0 ] != '\0' )
			snprintf ( authString, BUFSIZ, "username=%s,password=%s", auth->username, auth->password ? auth->password : "" );
		else
			snprintf ( authString, BUFSIZ, "password=" );
		snprintf ( mount_exec, BUFSIZ, "networkmount -t cifs \"//%s/%s\" \"%s\" -o %s,ip=%s,dir_mode=0755,file_mode=0644,uid=%d,gid=%d", IPtoString ( service->IP, IPBuffer, 16 ), service->share, path, authString, IPtoString ( service->IP, IPBuffer, 16 ), geteuid(), getegid() );
	}
	else
		snprintf ( mount_exec, BUFSIZ, "networkmount -t smbfs \"//%s/%s\" \"%s\" -o username=%s,password=%s,ip=%s,dmask=755,fmask=644,uid=%d,gid=%d,debug=0,ttl=%u", service->host, service->share, path, ( auth && auth->username ) ? auth->username : "", ( auth && auth->password ) ? auth->password : "", IPtoString ( service->IP, IPBuffer, 16 ), geteuid(), getegid(), smbTTL );

	output = execute ( mount_exec, STDOUT_FILENO | STDERR_FILENO, environ, &exitStatus );
	if ( !output )
		return -ENOMSG;

	if ( isMounted ( path ) )
	{
		free ( output );
		return EXIT_SUCCESS;
	}

	LNCdebug ( LNC_ERROR, 1,"SMBmount() -> isMounted( %s )", path );
	LNCdebug ( LNC_ERROR, 0, "SMBmount() -> execute( %s ): %s", mount_exec, output );
	error = SMBerror ( output );
	free ( output );
	errno = error ? error : ENOMSG;

	return -errno;
}

static  int SMBumount ( Service *service )
{
	char * output;
	char umount_exec[ BUFSIZ ];
	char path[ PATH_MAX ];
	int exitStatus;

	if ( service->flags & LAZY_UMOUNT )
		LNCdebug ( LNC_INFO, 0, "SMBumount(): forcing umount" );

	createServicePath ( path, PATH_MAX, service );
	snprintf ( umount_exec, BUFSIZ, "networkumount -t %s %s\"%s\"", useCifs ? "cifs" : "smbfs", ( service->flags & LAZY_UMOUNT ) ? "-l " : "", path );
	output = execute ( umount_exec, STDERR_FILENO | STDOUT_FILENO, environ, &exitStatus );
	if ( !output )
		return -ENOMSG;

	if ( ( exitStatus != 0 ) && ( exitStatus != 1 ) && ( exitStatus != 4 ) )
		LNCdebug ( LNC_ERROR, 0, "SMBumount(): exec = %s, exitStatus = %d, output:\n%s", umount_exec, exitStatus, output );

	free ( output );

	if ( isMounted ( path ) )
		return -ENOMSG;

	return EXIT_SUCCESS;
}

// static  char *SMBgetNetbiosName( char *ip ) {
// 	char * output;
// 	char netbios[ NAME_MAX ] = "";
// 	char *ptr;
// 	char exec[ BUFSIZ ];
// 	int exitStatus;
//
// 	snprintf( exec, BUFSIZ, "nmblookup -d0 -A -T %s", ip );
// 	output = execute( exec, STDOUT_FILENO | STDERR_FILENO, environ, &exitStatus );
// 	if ( !output ) {
// 		LNCdebug( LNC_ERROR, 0, "SMBgetDomainName( %s ) -> execute( %s ): No output available", ip, exec );
// 		errno = ENOMSG;
// 		return NULL;
// 	}
//
// 	if ( strstr( output, "ERROR:" ) ) {
// 		LNCdebug( LNC_ERROR, 0, "SMBgetNetbiosName() output:\n%s", output );
// 		goto error;
// 	}
//
// 	ptr = strstr( output, "Looking up status of" );
// 	if ( !ptr )
// 		goto nothingFound;
//
// 	ptr = strchr( ptr, '\n' );
// 	if ( !ptr || !( ++ptr ) )
// 		goto nothingFound;
//
// 	ptr++;
// 	if ( !ptr )
// 		goto nothingFound;
//
// 	sscanf( ptr, "\t%s<00> -         B <ACTIVE>", netbios );
// 	if ( netbios[ 0 ] == '\0' )
// 		goto nothingFound;
//
// 	free( output );
//
// 	return strdup( netbios );
//
// nothingFound:
// 	free( output );
// 	errno = ENODEV;
// 	return NULL;
//
// error:
// 	free( output );
// 	errno = ENOMSG;
// 	return NULL;
// }
//
// static  char *SMBgetDomainName( char *ip ) {
// 	char * output;
// 	char domain[ NAME_MAX ] = "";
// 	char *beginPtr, *endPtr;
// 	char exec[ BUFSIZ ];
// 	int exitStatus;
//
// 	snprintf( exec, BUFSIZ, "nmblookup -d0 -A -T %s", ip );
// 	output = execute( exec, STDOUT_FILENO | STDERR_FILENO, environ, &exitStatus );
// 	if ( !output ) {
// 		LNCdebug( LNC_ERROR, 0, "SMBgetDomainName( %s ) -> execute( %s ): No output available", ip, exec );
// 		errno = ENOMSG;
// 		return NULL;
// 	}
//
// 	if ( strstr( output, "ERROR:" ) ) {
// 		LNCdebug( LNC_ERROR, 0, "SMBgetDomainName() output:\n%s", output );
// 		goto error;
// 	}
//
// 	beginPtr = strstr( output, "Looking up status of" );
// 	if ( !beginPtr )
// 		goto nothingFound;
//
// 	beginPtr = strchr( beginPtr, '\n' );
// 	if ( !beginPtr || !( ++beginPtr ) )
// 		goto nothingFound;
//
// 	for ( endPtr = strchr( beginPtr, '\n' ); endPtr; beginPtr = ++endPtr, endPtr = strchr( beginPtr, '\n' ) ) {
// 		*endPtr = '\0';
// 		if ( strstr( beginPtr, "<00>" ) && strstr( beginPtr, "<GROUP>" ) ) {
// 			sscanf( beginPtr, "\t%15[^`] <00> - <GROUP> B <ACTIVE>", domain );
// 			if ( domain[ 0 ] == '\0' )
// 				goto nothingFound;
// 			goto out;
// 		}
// 		*endPtr = '\n';
// 	}
//
// nothingFound:
// 	free( output );
// 	errno = ENODEV;
// 	return NULL;
//
// error:
// 	free( output );
// 	errno = ENOMSG;
// 	return NULL;
//
// out:
// 	free( output );
// 	return strdup( removeTrailingWhites( domain ) );
// }
//
// static  int SMBgetDomains( Service *service ) {
// 	char * output;
// 	char *beginPtr, *endPtr;
// 	char ip[ DOMAINS_MAX ][ 16 ];
// 	int i, j;
// 	void *domain;
// 	Service *new;
// 	Service *found;
// 	char URL[ PATH_MAX ];
// 	pthread_t tid[ DOMAINS_MAX ];
// 	int exitStatus;
// 	int error = 0;
// 	char exec[ BUFSIZ ];
//
// 	( void ) service;
//
// 	strcpy( exec, "nmblookup -M -- -" );
// 	output = execute( exec, STDOUT_FILENO | STDERR_FILENO, environ, &exitStatus );
// 	if ( !output ) {
// 		LNCdebug( LNC_ERROR, 0, "SMBgetDomains( %s ) -> execute( %s ): No output available", service->network, exec );
// 		error = ENOMSG;
// 		goto out;
// 	}
//
// 	if ( strstr( output, "ERROR" ) ) {
// 		LNCdebug( LNC_ERROR, 0, "SMBgetDomains() output:\n%s", output );
// 		free( output );
// 		error = ENOMSG;
// 		goto out;
// 	}
//
// 	beginPtr = strstr( output, "querying" );
// 	if ( !beginPtr )
// 		goto out;
//
// 	beginPtr = strchr( beginPtr, '\n' );
// 	if ( !beginPtr || !( ++beginPtr ) )
// 		goto out;
//
// 	i = 0;
// 	for ( endPtr = strchr( beginPtr, '\n' ); endPtr; beginPtr = ++endPtr, endPtr = strchr( beginPtr, '\n' ) ) {
// 		*endPtr = '\0';
// 		memset( ip[ i ], 0, 16 );
// 		sscanf( beginPtr, "%[0-9.]", ip[ i ] );
// 		*endPtr = '\n';
// 		if ( ip[ i ][ 0 ] == '\0' )
// 			continue;
// 		tid[ i ] = ( pthread_t ) createThread( ( void * ( * ) ( void * ) ) SMBgetDomainName, ip[ i ], LOOP_MODE );
// 		i += 1;
// 	}
//
// 	for ( j = 0; j < i; ++j ) {
// 		pthread_join( tid[ j ], &domain );
// 		if ( domain == NULL )
// 			continue;
//
// 		snprintf( URL, PATH_MAX, "network://%s/%s", SMBprotocol, ( char * ) domain );
// 		found = findListEntry( browselist, URL );
// 		if ( found ) {
// 			updateServiceTimeStamp( found );
// 			putListEntry( browselist, found );
// 		} else {
// 			new = newServiceEntry( IPtoNumber( ip[ j ] ), domain, NULL, NULL, NULL, strdup( SMBprotocol ) );
// 			addListEntry( browselist, new );
// 		}
// 	}
//
// out:
// 	free( output );
// 	errno = error;
// 	return -error;
//
// }
/*
static  int SMBgetHosts( Service *service ) {
	char * output;
	char *ptr1, *ptr2;
	char ip[ 16 ];
	char URL[ PATH_MAX ];
	int i, j;
	void *netbios;
	char *tmp[ SERVERS_MAX + 1 ];
	char exec[ PATH_MAX ];
	pthread_t tid[ SERVERS_MAX + 1 ];
	int exitStatus;
	Service *new;
	Service *found;
	int error = 0;

	snprintf( exec, BUFSIZ, "nmblookup -d0 \"%s\"", service->domain );
	output = execute( exec, STDOUT_FILENO | STDERR_FILENO, environ, &exitStatus );
	if ( !output ) {
		LNCdebug( LNC_ERROR, 0, "SMBgetHosts( %s ) -> execute( %s ): No output available", service->domain, exec );
		error = ENOMSG;
		goto out;
	}

	if ( strstr( output, "ERROR" ) ) {
		LNCdebug( LNC_ERROR, 0, "SMBgetHosts() output:\n%s", output );
		free( output );
		error = ENOMSG;
		goto out;
	}

	ptr1 = strchr( output, '\n' );
	if ( !ptr1 )
		goto out;

	ptr1++;
	if ( !ptr1 )
		goto out;

	i = 0;
	for ( ptr2 = ptr1, ptr1 = strchr( ptr1, '\n' ); ptr1; ptr2 = ++ptr1, ptr1 = strchr( ptr1, '\n' ) ) {
		*ptr1 = '\0';
		memset( ip, 0, 16 );
		sscanf( ptr2, "%[0-9.]", ip );
		if ( ip[ 0 ] == '\0' )
			continue;

		tmp[ i ] = strdup( ip );
		tid[ i ] = ( pthread_t ) createThread( ( void * ( * ) ( void * ) ) SMBgetNetbiosName, tmp[ i ], LOOP_MODE );
		i += 1;
	}

	for ( j = 0; j < i; ++j ) {
		pthread_join( tid[ j ], &netbios );
		if ( netbios == NULL ) {
			free( tmp[ j ] );
			continue;
		}

		snprintf( URL, PATH_MAX, "network://%s/%s/%s", SMBprotocol, service->domain, ( char * ) netbios );
		found = findListEntry( browselist, URL );
		if ( found ) {
			updateServiceTimeStamp( found );
			updateServiceIPAddress( found, IPtoNumber( tmp[ j ] ) );
			putListEntry( browselist, found );
		} else {
			new = newServiceEntry( IPtoNumber( tmp[ j ] ), strdup( service->domain ), netbios, NULL, NULL, strdup( SMBprotocol ) );
			addListEntry( browselist, new );
		}

		free( tmp[ j ] );
	}

out:
	free( output );
	errno = error;
	return -error;
}*/

static  int SMBgetShares ( Service *service, struct auth *auth )
{
	char exec[ BUFSIZ ];
	char *output;
	char *ptr1, *ptr2;
	char share[ NAME_MAX + 1 ], type[ NAME_MAX + 1 ], comment[ NAME_MAX + 1 ];
	char URL[ PATH_MAX ];
	char *sharePtr, *typePtr, *commentPtr;
	char IPBuffer[ 16 ];
	char *IP;
	int cnt;
	int exitStatus;
	Service *new;
	Service *found;
	int error = 0;

	IP = IPtoString( service->IP, IPBuffer, 16 );
	if ( !IP )
		return -EINVAL;

	snprintf ( exec, BUFSIZ, "smbclient -d0 -N -L %s", IP );
	if ( auth && auth->username[ 0 ] != '\0' )
		snprintf ( exec+strlen ( exec ), BUFSIZ, " -U %s", auth->username );
	else
		snprintf ( exec+strlen ( exec ), BUFSIZ, " -U " );

	output = execute ( exec, STDOUT_FILENO | STDERR_FILENO, environ, &exitStatus );
	if ( !output )
	{
		LNCdebug ( LNC_ERROR, 0, "SMBgetShares( %s ) -> execute( %s ): No output available", service->host, exec );
		error = ENOMSG;
		goto out;
	}

	if ( exitStatus > 0 )
	{
		LNCdebug ( LNC_ERROR, 0, "SMBgetShares() -> execute(): Failed to execute \"%s\"", exec );
		error = ENOMSG;
		goto out;
	}

	error = SMBerror ( output );
	if ( error != 0 )
	{
		LNCdebug ( LNC_ERROR, 0, "SMBgetShares() -> execute( \"%s\" ): Failed to scan %s for shares, output:\n%s", exec, service->host, output );
		goto out;
	}

	for ( cnt = 0, ptr1 = strchr ( output, '\n' ), ptr2 = output; ptr1; ptr2 = ++ptr1, ptr1 = strchr ( ptr1, '\n' ) )
	{
		*ptr1 = share[ 0 ] = type[ 0 ] = '\0';
		sscanf ( ptr2, "\t%15[^`]%10[^`]%[^\n]", share, type, comment );

		typePtr = removeWhites ( type );
		if ( !typePtr || ( strncmp ( typePtr, "Disk", 4 ) != 0 ) )
			continue;

		sharePtr = removeWhites ( share );
		if ( sharePtr &&
		        ( strncasecmp ( sharePtr, "admin$", 6 ) != 0 ) &&
		        ( strncasecmp ( sharePtr, "print$", 6 ) != 0 ) )
		{

			if ( service->domain )
				snprintf ( URL, PATH_MAX, "network://%s/%s/%s/%s", SMBprotocol, service->domain, service->host, sharePtr );
			else
				snprintf ( URL, PATH_MAX, "network://%s/%s/%s", SMBprotocol, service->host, sharePtr );

			lockList ( browselist, WRITE_LOCK );
			found = findListEntryUnlocked ( browselist, URL );
			if ( !found )
			{
				commentPtr = removeWhites ( comment );
				if ( commentPtr && commentPtr[ 0 ] == '\0' )
					commentPtr = NULL;

				new = newServiceEntry ( service->IP, service->domain ? strdup ( service->domain ) : NULL, strdup ( service->host ), strdup ( sharePtr ), commentPtr ? strdup ( commentPtr ) : NULL, strdup ( SMBprotocol ) );
				addListEntryUnlocked ( browselist, new );
			} else {
				updateServiceTimeStamp( found );
				putListEntry( browselist, found );
			}
			unlockList( browselist );
		}
	}

out:
	free ( output );
	errno = error;
	return -error;
}

static int SMBmonitorConfiguration ( const char *path, const char *filename, int event )
{
	char *configEntry;

	LNCdebug ( LNC_DEBUG, 0, "SMBmonitorConfiguration(): path = %s, filename = %s, event = %d", path, filename, event );

	initKeys ( "LNC/plugins/SMB" );
	if ( event == FAMCreated || event == FAMChanged || event == FAMDeleted )
	{
		configEntry= getCharKey ( "LNC/plugins/SMB", "UseCifs", "True" );
		if ( strcasecmp ( configEntry, "True" ) == 0 )
			useCifs = 1;
		else
			useCifs = 0;

		free ( configEntry );

		smbTTL = getNumKey ( "LNC/plugins/SMB", "SMBTTL", smbTTL );

		LNCdebug ( LNC_INFO, 0, "SMBmonitorConfiguration(): using %s protocol", useCifs ? "CIFS" : "SMB" );
		LNCdebug ( LNC_INFO, 0, "SMBmonitorConfiguration(): TTL = %u", smbTTL );
	}

	cleanupKeys ( "LNC/plugins/SMB" );

	return EXIT_SUCCESS;
}

static  int SMBinit ( void )
{
	char *configEntry;
	char buffer[ BUFSIZ ];
	char path[ PATH_MAX ];
	struct passwd * user;

	initKeys ( "LNC/plugins/SMB" );
	configEntry= getCharKey ( "LNC/plugins/SMB", "UseCifs", "True" );
	if ( strcasecmp ( configEntry, "True" ) == 0 )
		useCifs = 1;
	else
		useCifs = 0;

	free ( configEntry );

	smbTTL = getNumKey ( "LNC/plugins/SMB", "SMBTTL", 1000 );
	setCharKey ( "LNC/plugins/SMB", "UseCifs", useCifs ? "True" : "False", USER_ROOT );
	setNumKey ( "LNC/plugins/SMB", "SMBTTL", smbTTL, USER_ROOT );
	cleanupKeys ( "LNC/plugins/SMB" );

	LNCdebug ( LNC_INFO, 0, "SMBinit(): using %s protocol", useCifs ? "CIFS" : "SMB" );
	LNCdebug ( LNC_INFO, 0, "SMBinit(): TTL = %u", smbTTL );

	user = getSessionInfoByUID ( getuid(), buffer );
	if ( !user )
	{
		LNCdebug ( LNC_ERROR, 1, "SMBinit() -> getSessionInfoByUID( %d )", getuid() );
		return -EXIT_FAILURE;
	}

	snprintf ( path, PATH_MAX, "%s/%s/user/sw/LNC/plugins/SMB", user->pw_dir, KDB_DB_USER );
	addFileToMonitorList ( path, SMBmonitorConfiguration );

	return 0;
}

static  void SMBcleanUp ( void )
{
	char buffer[ BUFSIZ ];
	char path[ PATH_MAX ];
	struct passwd * user;

	user = getSessionInfoByUID ( getuid(), buffer );
	if ( !user )
	{
		LNCdebug ( LNC_ERROR, 1, "SMBcleanUp() -> getSessionInfoByUID( %d )", getuid() );
		return;
	}

	snprintf ( path, PATH_MAX, "%s/%s/user/sw/LNC/plugins/SMB", user->pw_dir, KDB_DB_USER );
	delFileFromMonitorList ( path, SMBmonitorConfiguration );
}

static NetworkPlugin smb =
{
	.minimumTouchDepth = 6,
	.supportsAuthentication = 1,
	.init = SMBinit,
	.cleanUp = SMBcleanUp,
	.networkName = SMBnetworkName,
	.description = SMBdescription,
	.protocol = SMBprotocol,
	.portNumber = 139,
	.mount = SMBmount,
	.umount = SMBumount,
//	.getDomains = SMBgetDomains,
//	.getHosts = SMBgetHosts,
	.getShares = SMBgetShares,
};

void *giveNetworkPluginInfo ( void )
{
	strlcpy ( SMBnetworkName, gettext ( "Windows Network" ), NAME_MAX + 1 );
	return & smb;
}
