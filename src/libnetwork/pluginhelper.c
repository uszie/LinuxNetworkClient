/***************************************************************************
*   Copyright (C) 2003 by Usarin Heininga                                 *
*   usarin@heininga.nl                                                    *
*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License as published by  *
*   the Free Software Foundation; either version 2 of the License, or     *
*   (at your option) any later version.                                   *
***************************************************************************/

#include "pluginhelper.h"
#include "networkclient.h"
#include "nconfig.h"
#include "user.h"
#include "dir.h"

#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <dirent.h>
#include <signal.h>
#include <mntent.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

//struct simpleNetworkPluginList __simpleNetworkPluginList;
struct browselist __browselist;

//struct simpleNetworkPluginList *simpleNetworkPluginList = &__simpleNetworkPluginList;
struct browselist *browselist = &__browselist;

char realRoot[ PATH_MAX ];
int realRootLength;

struct expired_struct
{
	char name[ NAME_MAX + 1 ];
	char *key;
	char keyLen;
};

static int tryToSetBusy ( Service *service );

static char *spaceCalculator ( char *buf, size_t size, Service *service );
static char *printStatus ( char *buf, size_t size, Service *service );
static char *printExpireTime ( char *buf, size_t size, Service *service );

static int testMount ( Service *service );
static int mount ( Service *service, struct auth *auth );
static int umount ( Service *service );
static int getDomains ( Service *service );
static int getHosts ( Service *service );
static int getShares ( Service *service, struct auth *auth );
static int pingScan ( const char *hostname, int port, int forceRescan );

void setBusy ( Service *service )
{
	int retval;
	retval = pthread_rwlock_wrlock ( &service->busyLock );
	if ( retval != 0 )
	{
		errno = retval;
		LNCdebug ( LNC_ERROR, 1, "setBusy() -> pthread_rwlock_wrlock()" );
		printf ( "setBusy() -> pthread_rwlock_wrlock()\n" );
		abort();
	}

}

void unsetBusy ( Service *service )
{
	int retval;
	service->flags = 0;
	retval = pthread_rwlock_unlock ( &service->busyLock );
	if ( retval != 0 )
	{
		errno = retval;
		LNCdebug ( LNC_ERROR, 1, "unsetBusy() -> pthread_rwlock_unlock()" );
		printf ( "unsetBusy() -> pthread_rwlock_unlock()\n" );
		abort();
	}
}

static int tryToSetBusy ( Service *service )
{
	int retval;

	retval = pthread_rwlock_trywrlock ( &service->busyLock );
	if ( retval != 0 && retval != EBUSY )
	{
		LNCdebug ( LNC_ERROR, 1, "tryToSetBusy() -> pthread_rwlock_trywrlock()" );
		return 0;
	}
	else if ( retval == EBUSY )
	{
		char buffer[ PATH_MAX ];

		LNCdebug ( LNC_INFO, 0, "tryToSetBusy() -> pthread_rwlock_trywrlock(): Service is already busy ( %s )", createServiceKey ( buffer, PATH_MAX, service, 1 ) );
		return 0;
	}

	return 1;
}

// void tryToSetBusy2 ( Service *service )
// {
// 	int retval;
//
// 	while ( 1 )
// 	{
// 		retval = pthread_rwlock_trywrlock ( &service->busyLock );
// 		if ( retval != 0 && retval != EBUSY )
// 		{
// 			LNCdebug ( LNC_ERROR, 1, "tryToSetBusy() -> pthread_rwlock_trywrlock()" );
// 			printf ( "tryToSetBusy2() -> pthread_rwlock_trywrlock()\n" );
// 			LNCcleanUp();
// 			exit ( EXIT_FAILURE );
// 			return;
// 		}
// 		else if ( retval == EBUSY )
// 		{
// 			printf ( "tryToSetBusy2() -> pthread_rwlock_trywrlock(): waiting for lock\n" );
// 			sleep ( 1 );
// 			continue;;
// 		}
// 		break;
// 	}
//
// 	return;
// }

char *IPtoString ( u_int32_t ip, char *buffer, size_t size )
{
	struct in_addr p;
	char *ipPtr;

	p.s_addr = ( in_addr_t ) ip;
	ipPtr = ( char * ) inet_ntop( AF_INET, ( const void * ) &p, buffer, size );
	if ( !ipPtr )
	{
		LNCdebug ( LNC_ERROR, 0, "IPtoString( %d ) -> inet_ntoa()", ip );
		return NULL;
	}

	return ipPtr;
}

u_int32_t IPtoNumber ( const char *ip )
{
	int retval;
	struct in_addr p;

	retval = inet_aton ( ip, &p );
	if ( retval == 0 )
	{
		LNCdebug ( LNC_ERROR, 0, "IPtoNumber( %s ) -> inet_aton(): Invalid IP Address (%s)", ip, ip );
		return 0;
	}

	return ( u_int32_t ) p.s_addr;
}

u_int32_t getIPAddress ( const char *hostname )
{
	int retval;
	int error;
	char buffer[ BUFSIZ ];
	struct hostent hostent;
	struct hostent *result;

	retval = gethostbyname_r ( hostname, &hostent, buffer, BUFSIZ, &result, &error );
	if ( retval != 0 || !result )
	{
		errno = ENODEV;
		LNCdebug ( LNC_ERROR, 0, "getIPAddress( %s ) -> gethostbyname_r(): %s", hostname, hstrerror ( error ) );
		return 0;
	}

	return ( u_int32_t ) ( ( struct in_addr * ) result->h_addr_list[ 0 ] )->s_addr;
}

int isInternetDomain ( const char *host )
{
	char *ptr;
	int len;

	ptr = strrchr ( host, '.' );
	if ( ptr && * ( ptr+1 ) != '\0' )
	{
		len = strlen ( ptr+1 );
		if ( len == 2 || len == 3 )
			return 1;
	}


	return 0;
}

char *getHostName ( u_int32_t IP, char *buffer, size_t bufferSize, int mode )
{
	struct hostent * hostent;
	struct in_addr tmp;
	char *ptr;

	tmp.s_addr = IP;
	hostent = gethostbyaddr ( ( void * ) & tmp, sizeof ( struct in_addr ), AF_INET );
	if ( !hostent )
	{
		strlcpy ( buffer, inet_ntoa ( tmp ), bufferSize );
		return buffer;
	}

	if ( mode == SHORT_HOSTNAME && !isInternetDomain ( hostent->h_name ) )
	{
		ptr = strchr ( hostent->h_name, '.' );
		if ( ptr )
			* ptr = '\0';
	}

	strlcpy ( buffer, hostent->h_name, bufferSize );

	return buffer;
}

char *getDomainName ( u_int32_t IP, char *buffer, size_t bufferSize )
{
	struct hostent * hostent;
	struct in_addr tmp;
	char *ptr;

	tmp.s_addr = IP;
	hostent = gethostbyaddr ( ( void * ) & tmp, sizeof ( struct in_addr ), AF_INET );
	if ( !hostent )
		return NULL;

	if ( isInternetDomain ( hostent->h_name ) )
		return NULL;

	ptr = strchr ( hostent->h_name, '.' );
	if ( !ptr || * ( ptr + 1 ) == '\0' )
		return NULL;

	strlcpy ( buffer, ptr + 1, bufferSize );

	return buffer;
}

static char *spaceCalculator ( char *buf, size_t size, Service *service )
{
	int len;

	if ( service->share )
		snprintf ( buf, size, "network://%s/%s/%s", service->network, service->host, service->share );
	else if ( service->host )
		snprintf ( buf, size, "network://%s/%s", service->network, service->host );
	else
		snprintf ( buf, size, "network://%s", service->network );

	len = strlen ( buf );
	len = 50 - len;
	len = ( len < 0 ) ? 0 : len;

	memset ( buf, ' ', len );
	buf[ len ] = '\0';

	return buf;
}

Service *newServiceEntry ( u_int32_t IP, char *domain, char *host, char *share, char *comment, char *network )
{
	char *ptr;
	char *buffer;
	int cnt;
	int i;
	struct hostent * hostent;
	struct in_addr tmp;
	size_t size;
	Service *new;

	new = malloc ( sizeof ( Service ) );
	new->IP = IP;
	new->domain = domain;
	new->host = host;
	new->share = share;
	new->network = network;

	if ( comment )
		new->comment = comment;
	else
	{
		if ( share )
			new->comment = strdup ( "Network Disk" );
		else if ( host )
			new->comment = strdup ( "Network Host" );
		else if ( domain )
			new->comment = strdup ( "Network Domain" );
		else if ( network )
			new->comment = strdup ( "Network Protocol" );
		else
			new->comment = strdup ( "Network Home" );
	}

	size =  PATH_MAX * sizeof( char );
	cnt = 0;
	buffer = malloc( size );
	ptr = createServiceKey( buffer, size, new, 1 );
	new->aliases[ cnt ] = ptr;
	++cnt;

	if ( domain ) {
		buffer = malloc( size );
		ptr = createServiceKey( buffer, size, new, 3 );
		new->aliases[ cnt ] = ptr;
		++cnt;
	}

	if ( IP ) {
		if ( domain || !isValidIP( host ) ) {
			buffer = malloc( size );
			ptr = createServiceKey( buffer, size, new, 2 );
			new->aliases[ cnt ] = ptr;
			++cnt;
		}

		buffer = malloc( size );
		ptr = createServiceKey( buffer, size, new, 4 );
		new->aliases[ cnt ] = ptr;
		++cnt;

		tmp.s_addr = IP;
		hostent = gethostbyaddr ( ( void * ) & tmp, sizeof ( struct in_addr ), AF_INET );
		if ( !hostent )
			LNCdebug( LNC_ERROR, 1, "newServiceEntry( %u, %s, %s, %s, %s, %s ) -> gethostbyaddr()", IP, domain, host, share, comment, network );
		else if ( hostent->h_aliases ){
			new->domain = NULL;
			for ( i = 0; hostent->h_aliases[ i ] && i < MAXALIASES; ++i ) {
				new->host = hostent->h_aliases[ i ];
				buffer = malloc( size );
				ptr = createServiceKey( buffer, size, new, 1 );
				new->aliases[ cnt ] = ptr;
				++cnt;
			}
			new->domain = domain;
			new->host = host;
		}
	}

	new->aliases[ cnt ] = NULL;

	new->badConnection = 0;
	new->error = 0;
	new->status = NEEDS_ACTION;
	new->expireTime = 0;
	new->lastTimeUsed = getProgramTime();
	new->flags = 0;
	new->hostType = AUTO_HOST;
	pthread_rwlock_init ( &new->busyLock, NULL );

	return new;
}

int isMounted ( const char *path )
{
	char buf[ BUFSIZ ];
	struct mntent tmp, *entry;
	FILE *mtab;

	mtab = setmntent ( "/proc/self/mounts", "r" );
	if ( !mtab )
	{
		LNCdebug ( LNC_ERROR, 1, "isMounted( %s ) -> setmntent( %s, r)", path, _PATH_MOUNTED );
		return -EXIT_FAILURE;
	}

	while ( ( entry = getmntent_r ( mtab, &tmp, buf, BUFSIZ ) ) )
	{
		if ( strcmp ( entry->mnt_dir, path ) == 0 )
		{
			endmntent ( mtab );
			return 1;
		}
	}

	endmntent ( mtab );

	errno = ENOENT;
	return 0;
}

int isMountInUse ( const char *path )
{
	char buffer[ PATH_MAX ];
	char cwd[ PATH_MAX ];
	struct dirent *taskEntry, *fdEntry;
	DIR *taskDir, *fdDir;
	int retval;
	int len = strlen ( path );

	LNCdebug ( LNC_INFO, 0, "isMountInUse(): Checking path %s", path );
	taskDir = opendir ( "/proc/self/task" );
	if ( !taskDir )
	{
		LNCdebug ( LNC_ERROR, 1, "isMountInUse() -> opendir( %s )", buffer );
		return -EXIT_FAILURE;
	}

	while ( ( taskEntry = readdir ( taskDir ) ) )
	{
		if ( taskEntry->d_name[ 0 ] == '.' )
			continue;

		snprintf ( cwd, PATH_MAX, "/proc/self/task/%s/cwd", taskEntry->d_name );
		retval = readlink ( cwd, buffer, PATH_MAX );
		if ( retval > 0 )
		{
			buffer[ retval ] = '\0';
			if ( strncmp ( buffer, path, len ) == 0 )
			{
				LNCdebug ( LNC_INFO, 0, "Path %s is used by pid %s, type = CWD:%s", path, taskEntry->d_name, buffer );
				closedir ( taskDir );
				return 1;
			}
		}

		sprintf ( buffer, "/proc/self/task/%s/fd", taskEntry->d_name );
		fdDir = opendir ( buffer );
		if ( !fdDir )
		{
			LNCdebug ( LNC_ERROR, 1, "isMountInUse() -> opendir( %s )", buffer );
			closedir ( taskDir );
			return -EXIT_FAILURE;
		}

		while ( ( fdEntry = readdir ( fdDir ) ) )
		{
			if ( fdEntry->d_name[ 0 ] == '.' )
				continue;

			sprintf ( cwd, "/proc/self/task/%s/fd/%s", taskEntry->d_name, fdEntry->d_name );
			retval = readlink ( cwd, buffer, PATH_MAX );
			if ( retval > 0 )
			{
				buffer[ retval ] = '\0';
				if ( strncmp ( buffer, path, len ) == 0 )
				{
					LNCdebug ( LNC_INFO, 0, "Path %s is used by pid %s, type = FD:%s", path, taskEntry->d_name, buffer );
					closedir ( taskDir );
					closedir ( fdDir );
					return 1;
				}
			}
		}
		closedir ( fdDir );
	}
	closedir ( taskDir );

	return 0;
}

char *createServiceKey ( char *buffer, size_t size, Service *service, int mode )
{
	char *ptr;
	char *IP;
	char IPBuffer[ 16 ];

	ptr = memccpy ( buffer, "network:/", '\0', size );
	if ( !ptr )
		return NULL;

	* ( ptr -1 ) = '/';
	if ( !service->network )
		goto out;

	ptr = memccpy ( ptr, service->network, '\0', size - ( ptr - buffer ) );
	if ( !ptr )
		return NULL;

	if ( mode < 1 || mode > 4 )
		mode = 1;

	if ( mode == 2 && !service->IP )
		mode = 1;

	if (  mode == 1 )
	{
		if ( service->domain ) {
			* ( ptr -1 ) = '/';
			ptr = memccpy ( ptr, service->domain, '\0', size - ( ptr - buffer ) );
			if ( !ptr )
				return NULL;
		}

		if ( !service->host )
			goto out;

		* ( ptr -1 ) = '/';
		ptr = memccpy ( ptr, service->host, '\0', size - ( ptr - buffer ) );
		if ( !ptr )
			return NULL;

		if ( !service->share )
			goto out;

		* ( ptr -1 ) = '/';
		ptr = memccpy ( ptr, service->share, '\0', size - ( ptr - buffer ) );
		if ( !ptr )
			return NULL;
	}
	else if ( mode == 3 )
	{
		if ( service->host ) {
			* ( ptr -1 ) = '/';
			ptr = memccpy ( ptr, service->host, '\0', size - ( ptr - buffer ) );
			if ( !ptr )
				return NULL;
		}

		if ( service->domain ) {
			if ( service->host )
				* ( ptr -1 ) = '.';
			else
				* ( ptr - 1 ) = '/';
			ptr = memccpy ( ptr, service->domain, '\0', size - ( ptr - buffer ) );
			if ( !ptr )
				return NULL;
		}

		if ( !service->share || !service->host )
			goto out;

		* ( ptr -1 ) = '/';
		ptr = memccpy ( ptr, service->share, '\0', size - ( ptr - buffer ) );
		if ( !ptr )
			return NULL;
	}
	else if ( mode == 2 )
	{
		* ( ptr -1 ) = '/';
		IP = IPtoString ( service->IP, IPBuffer, 16 );
		if ( !IP )
			return NULL;

		ptr = memccpy ( ptr, IP, '\0', size - ( ptr - buffer ) );
		if ( !ptr )
			return NULL;

		if ( !service->share )
			goto out;

		* ( ptr -1 ) = '/';
		ptr = memccpy ( ptr, service->share, '\0', size - ( ptr - buffer ) );
		if ( !ptr )
			return NULL;
	}
	else if ( mode == 4 )
	{
		if ( service->domain ) {
			* ( ptr -1 ) = '/';
			ptr = memccpy ( ptr, service->domain, '\0', size - ( ptr - buffer ) );
			if ( !ptr )
				return NULL;
		}

		if ( !service->host )
			goto out;

		* ( ptr -1 ) = '/';
		ptr = memccpy ( ptr, service->host, '\0', size - ( ptr - buffer ) );
		if ( !ptr )
			return NULL;

		* ( ptr -1 ) = '_';
		IP = IPtoString( service->IP, IPBuffer, 16);
		if ( !IP )
			return NULL;

		ptr = memccpy ( ptr, IP, '\0', size - ( ptr - buffer ) );
		if ( !ptr )
			return NULL;

		if ( !service->share )
			goto out;

		* ( ptr -1 ) = '/';
		ptr = memccpy ( ptr, service->share, '\0', size - ( ptr - buffer ) );
		if ( !ptr )
			return NULL;
	}

out:
	buffer[ size - 1 ] = '\0';

	return buffer;
}

// char *createServiceKey ( char *buffer, size_t size, Service *service, int mode )
// {
// 	if ( service->share )
// 	{
// 		if ( service->domain && mode == 1 )
// 			snprintf ( buffer, size, "network://%s/%s/%s/%s", service->network, service->domain, service->host, service->share );
//
// 		else if ( service->domain && mode == 3 )
// 			snprintf ( buffer, size, "network://%s/%s.%s/%s", service->network, service->host, service->domain, service->share );
// 		else
//  			snprintf ( buffer, size, "network://%s/%s/%s", service->network, mode == 1 ? service->host : IPtoString ( service->IP ), service->share );
// 	}
// 	else if ( service->host )
// 	{
// 		if ( service->domain && mode == 1 )
// 			snprintf ( buffer, size, "network://%s/%s/%s", service->network, service->domain, service->host );
// 		else if ( service->domain && mode == 3 )
//  			snprintf ( buffer, size, "network://%s/%s.%s", service->network, service->host, service->domain );
// 		else
//  			snprintf ( buffer, size, "network://%s/%s", service->network, mode == 1 ? service->host : IPtoString ( service->IP ) );
// 	}
// 	else if ( service->domain )
//  		snprintf ( buffer, size, "network://%s/%s", service->network, service->domain );
// 	else
//  		snprintf ( buffer, size, "network://%s", service->network );
//
//
// 	return buffer;
// }

char *createServicePath ( char *buffer, size_t size, Service *service )
{
	char IPBuffer[ 16 ];
	char *IP;

	if ( service->IP ) {
		IP = IPtoString( service->IP, IPBuffer, 16 );
		if ( !IP ) {
			strcpy( IPBuffer, "0.0.0.0" );
			IP = IPBuffer;
		}
	}

	if ( service->share )
	{
		if ( service->domain )
			snprintf ( buffer, size, "%s/%s/%s/%s_%s/%s", realRoot, service->network, service->domain, service->host, IP, service->share );
		else
			snprintf ( buffer, size, "%s/%s/%s_%s/%s", realRoot, service->network, service->host, IP, service->share );
	}
	else if ( service->host )
	{
		if ( service->domain )
			snprintf ( buffer, size, "%s/%s/%s/%s_%s", realRoot, service->network, service->domain, service->host, IP );
		else
			snprintf ( buffer, size, "%s/%s/%s_%s", realRoot, service->network, service->host, IP );
	}
	else if ( service->domain )
		snprintf ( buffer, size, "%s/%s/%s", realRoot, service->network, service->domain );
	else if ( service->network )
		snprintf ( buffer, size, "%s/%s", realRoot, service->network );
	else
		snprintf ( buffer, size, "%s", realRoot );

	return buffer;
}

void createRealPath ( char *path, const char *URL )
{
	char *begin;
	char *end;
	char buffer[ NAME_MAX + 1];
	int i, len, done;
	Service *found;

	done = 0;
	i = 0;
	snprintf ( buffer, PATH_MAX, "%s", URL );
	begin = buffer;
	while ( ( begin = strchr ( begin, '/' ) ) )
	{
		++i;
		++begin;
		if ( i == 3 )
			break;
	}

	if ( i == 3 && *begin != '\0' )
	{
		end = strchr ( begin, '/' );
		if ( end )
			*end = '\0';


		found = findListEntry ( browselist, buffer );
		if ( found )
		{
			if ( found->domain && !found->host && end && ( *( end+1 ) != '\0' ) ) {
				*end = '/';
				begin = end+1;
				end = strchr ( begin, '/' );
				if ( end )
					*end = '\0';
				putListEntry( browselist, found );
				found = findListEntry ( browselist, buffer );
			}

			if ( found ) {
				createServicePath ( path, PATH_MAX, found );
				if ( end )
				{
					len = strlen ( path );
					*end = '/';
					snprintf ( path + len, PATH_MAX - len, "%s", end );
				}
				done = 1;
				putListEntry ( browselist, found );

			}

		}

	}

	if ( !done )
	{
		strlcpy ( path, realRoot, PATH_MAX );
		strlcpy ( path+realRootLength, URL+9, PATH_MAX-realRootLength );
	}

}

static void netonInsert ( Service *service )
{
	char path[ PATH_MAX ];
	int retval;

	createServicePath( path, PATH_MAX, service );
	retval = mkdir ( path, 0755 );
	if ( retval < 0 && errno != EEXIST )
		LNCdebug ( LNC_ERROR, 1, "netonInsert() -> mkdir( \"%s\")", path );

}

static void netonUpdate ( Service *update, Service *old )
{
	int retval;

	if ( old->status == SERVICE_MOUNTED )
	{
		old->flags |= LAZY_UMOUNT;
		retval = umount ( old );
		if ( retval < 0 )
			LNCdebug ( LNC_ERROR, 0, "netonRemove() -> umount(): Umount failed cannot remove directory" );
	}
}

static void netonRemove ( Service *service )
{
	char buffer[ PATH_MAX ];
	char *path;
	int retval;

	if ( service->status == SERVICE_MOUNTED )
	{
		service->flags |= LAZY_UMOUNT;
		retval = umount ( service );
		if ( retval < 0 )
		{
			LNCdebug ( LNC_ERROR, 0, "netonRemove() -> umount(): Umount failed cannot remove directory" );
			return ;
		}
	}

	path = createServicePath( buffer, PATH_MAX, service );
	retval = rmdir ( path );
	if ( retval < 0 )
		LNCdebug ( LNC_ERROR, 1, "netonRemove() -> rmdir( \"%s\")", path );
}

static char *printStatus ( char *buf, size_t size, Service *service )
{
	int status = service->status;

	if ( status == SCANNED )
		snprintf ( buf, size, "SCANNED      " );
	else if ( status == SERVICE_MOUNTED )
		snprintf ( buf, size, "MOUNTED      " );
	else if ( status == NEEDS_ACTION )
		snprintf ( buf, size, "NEEDS_ACTION " );
	else
		snprintf ( buf, size, "UNDEFINED   " );

	return buf;
}

static char *printExpireTime ( char *buf, size_t size, Service *service )
{
	if ( !service->expireTime )
		snprintf ( buf, size, "0000000000000000" );
	else
		snprintf ( buf, size, "%u", service->expireTime );

	return buf;
}

static void netPrint ( Service *service )
{
	char buf1[ BUFSIZ ];
	char buf2[ BUFSIZ ];
	char buf3[ BUFSIZ ];
	char IPBuffer[ 16 ];

	if ( service->share )
		if ( service->domain )
			LNCdebug ( LNC_INFO, 0, "network://%s/%s/%s/%s%s Expire Time = \"%s\" Status = \"%s\" IP = \"%s\"", service->network, service->domain, service->host, service->share, spaceCalculator ( buf3, BUFSIZ, service ), printExpireTime ( buf2, BUFSIZ, service ), printStatus ( buf1, BUFSIZ, service ), IPtoString ( service->IP, IPBuffer, 16 ) );
		else
			LNCdebug ( LNC_INFO, 0, "network://%s/%s/%s%s Expire Time = \"%s\" Status = \"%s\" IP = \"%s\"", service->network, service->host, service->share, spaceCalculator ( buf3, BUFSIZ, service ), printExpireTime ( buf2, BUFSIZ, service ), printStatus ( buf1, BUFSIZ, service ), IPtoString ( service->IP, IPBuffer, 16 ) );
	else if ( service->host )
		if ( service->domain )
			LNCdebug ( LNC_INFO, 0, "network://%s/%s/%s%s Expire Time = \"%s\" Status = \"%s\" IP = \"%s\"", service->network, service->domain, service->host, spaceCalculator ( buf3, BUFSIZ, service ), printExpireTime ( buf2, BUFSIZ, service ), printStatus ( buf1, BUFSIZ, service ), IPtoString ( service->IP, IPBuffer, 16 ) );
		else
			LNCdebug ( LNC_INFO, 0, "network://%s/%s%s Expire Time = \"%s\" Status = \"%s\" IP = \"%s\"", service->network, service->host, spaceCalculator ( buf3, BUFSIZ, service ), printExpireTime ( buf2, BUFSIZ, service ), printStatus ( buf1, BUFSIZ, service ), IPtoString ( service->IP, IPBuffer, 16 ) );
	else if ( service->domain )
		LNCdebug ( LNC_INFO, 0, "network://%s/%s%s Expire Time = \"%s\" Status = \"%s\" IP = \"%s\"", service->network, service->domain, spaceCalculator ( buf3, BUFSIZ, service ), printExpireTime ( buf2, BUFSIZ, service ), printStatus ( buf1, BUFSIZ, service ), IPtoString ( service->IP, IPBuffer, 16 ) );
	else
		LNCdebug ( LNC_INFO, 0, "network://%s%s Expire Time = \"%s\" Status = \"%s\"", service->network, spaceCalculator ( buf3, BUFSIZ, service ), printExpireTime ( buf2, BUFSIZ, service ), printStatus ( buf1, BUFSIZ, service ) );
}

static void netFree ( Service *service )
{
	int i;

	if ( service->network )
		free ( service->network );
	if ( service->comment )
		free ( service->comment );
	if ( service->domain )
		free ( service->domain );
	if ( service->host )
		free ( service->host );
	if ( service->share )
		free ( service->share );

	if ( service->aliases ) {
		for ( i = 0; service->aliases[ i ]; ++i )
			free( service->aliases[ i ] );
	}

	pthread_rwlock_destroy ( &service->busyLock );
	free ( service );
}

static int netFind ( const char *search, Service *entry )
{
	int i;
	int cmp;

	for ( i = 0; entry->aliases[ i ]; ++i ) {
		cmp = strcmp( search, entry->aliases[ i ] );
		if ( cmp == 0 )
			return 0;
	}

	return 1;
}

static int netCompare ( Service *search, Service *entry )
{
	char searchKey[ PATH_MAX ];
	char entryKey[ PATH_MAX ];

	createServiceKey ( searchKey, PATH_MAX, search, 1 );
	createServiceKey ( entryKey, PATH_MAX, entry, 1 );

	return strcmp ( searchKey, entryKey );
}

static int breakPendingService ( const char *URL, char *network, char *domain, char *host, char *share )
{
	char cpy[ PATH_MAX ];
	char *ptr;
	char *tmp;
	char *token;
	int depth;

	strlcpy ( cpy, URL, PATH_MAX );

	ptr = cpy;
	for ( depth = 0; ( token = strtok_r ( ptr, "/", &tmp ) ); ++depth )
	{
		ptr = NULL;
		switch ( depth )
		{
			case 0:	continue;
			case 1:	snprintf ( network, PATH_MAX, "network://%s", token );
				continue;
			case 2: snprintf ( domain, PATH_MAX, "%s/%s", network, token );
				continue;
			case 3:	snprintf ( host, PATH_MAX, "%s/%s", domain, token );
				continue;
			case 4:	snprintf ( share, PATH_MAX, "%s/%s", host, token );
				continue;
			case 5:	return ++depth;
		}
	}

	return depth;
}

void updateServiceTimeStamp ( Service *service )
{
	service->lastTimeUsed = getProgramTime();
}

void updateServiceIPAddress ( Service *service, u_int32_t IP )
{
	service->IP = IP;
}

int getServiceDepth ( Service *service )
{
	if ( service->share )
	{
		if ( service->domain )
			return 5;
		else return 4;
	}
	else if ( service->host )
	{
		if ( service->domain )
			return 4;
		else
			return 3;
	}
	else if ( service->domain )
		return 3;
	else if ( service->network )
		return 2;

	return 0;
}

void deleteChildEntries( Service *service ) {
	char *URL;
	char *entryURL;
	char buffer[ PATH_MAX ];
	char entryBuffer[ PATH_MAX ];
	int length;
	int depth;
	int entryDepth;
	Service * entry;

	URL = createServiceKey ( buffer, PATH_MAX, service, 1 );
	length = strlen ( URL );
	depth = getServiceDepth ( service );
	forEachListEntry ( browselist, entry ) {
		entryURL = createServiceKey ( entryBuffer, PATH_MAX, entry, 1 );
		if ( strncmp ( URL, entryURL, length ) != 0 )
			continue;

		entryDepth = getServiceDepth ( entry );
		if ( entryDepth != ( depth + 1 ) )
			continue;

		LNCdebug ( LNC_INFO, 0, "deleteChildEntries( %s ): removing %s", URL, entryURL );
		deleteChildEntries( entry );
		__tmp__entry = entry->list.next;
		delListEntryUnlocked( browselist, entry );
	}
}

void removeExpiredBrowselistEntries ( u_int32_t timeStamp, Service *service, int deepRemove )
{
	char *URL;
	char *entryURL;
	char expiredTree[ PATH_MAX ];
	char buffer[ PATH_MAX ];
	char entryBuffer[ PATH_MAX ];
	int length;
	int depth;
	int entryDepth;
	Service * entry;

	URL = createServiceKey ( buffer, PATH_MAX, service, 1 );
	length = strlen ( URL );
	depth = getServiceDepth ( service );
	expiredTree[ 0 ] = '\0';

	lockList ( browselist, WRITE_LOCK );
	forEachListEntry ( browselist, entry )
	{
		if ( entry->hostType == MANUAL_HOST )
			continue;

		if ( entry->lastTimeUsed >= timeStamp )
			continue;

		entryURL = createServiceKey ( entryBuffer, PATH_MAX, entry, 1 );
		if ( strncmp ( URL, entryURL, length ) != 0 )
			continue;

		entryDepth = getServiceDepth ( entry );
		if ( entryDepth == depth )
			continue;
		else if ( ( entryDepth == ( depth + 1 ) ) || ( ( entryDepth == ( depth + 2 ) ) && ( depth == 2 ) && deepRemove && entry->domain ) )
		{
			LNCdebug ( LNC_INFO, 0, "removeExpiredBrowselistEntries( %s ): removing %s", URL, entryURL );
			deleteChildEntries( entry );
			__tmp__entry = entry->list.next;
			delListEntryUnlocked ( browselist, entry );
		}

	}
	unlockList ( browselist );
}

static int testMount ( Service *service )
{
	char realPath[ PATH_MAX ];
	char URL[ PATH_MAX ];
	int retval;
	struct stat stbuf;
	DIR *dir;
	struct dirent *dirent;

	createServiceKey ( URL, PATH_MAX, service, 1 );
	createRealPath ( realPath, URL );

	markSyscallStartTime();
	retval = stat ( realPath, &stbuf );
	if ( retval < 0 )
	{
		handleSyscallError ( URL );
		LNCdebug ( LNC_ERROR, 1, "testMount( %s ) -> stat( %s )", URL, realPath );
		return -errno;
	}

	dir = opendir ( realPath );
	if ( !dir )
	{
		handleSyscallError ( URL );
		LNCdebug ( LNC_ERROR, 1, "testMount( %s ) -> opendir( %s )", URL, realPath );
		return -errno;
	}

	dirent = readdir ( dir );
	if ( !dirent )
	{
		handleSyscallError ( URL );
		LNCdebug ( LNC_ERROR, 1, "testMount( %s ) -> readdir( %s )", URL, realPath );
		closedir ( dir );
		return -errno;
	}

	closedir ( dir );

	return EXIT_SUCCESS;
}

static int mount ( Service *service, struct auth *auth )
{
	int retval;
	char errorString[ BUFSIZ ];
	char URL[ PATH_MAX ];
	NetworkPlugin *plugin;
	u_int32_t currentTime = getProgramTime();

	createServiceKey ( URL, PATH_MAX, service, 1 );
	LNCdebug ( LNC_INFO, 0, "Mounting %s, username = \"%s\", password = \"%s\"", URL, auth->username, auth->password );

	plugin = findNetworkPlugin ( service->network );
	if ( !plugin )
	{
		LNCdebug ( LNC_ERROR, 1, "mount() -> findNetworkPlugin( %s )", service->network );
		return -EXIT_FAILURE;
	}

	retval = plugin->mount ( service, auth );
	LNCdebug ( LNC_INFO, 0, "mount( %s ) = %d ( %s )", URL, retval, strerror_r ( retval * -1, errorString, BUFSIZ ) );
	putNetworkPlugin ( plugin );
	if ( retval == 0 )
	{
		retval = testMount ( service );
		if ( retval != 0 )
			umount ( service );
	}
	service->lastTimeUsed = currentTime;
	return retval;
}

static int umount ( Service *service )
{
	int retval;
	char errorString[ BUFSIZ ];
	char URL[ PATH_MAX ];
	NetworkPlugin *plugin;

	createServiceKey ( URL, PATH_MAX, service, 1 );
	LNCdebug ( LNC_INFO, 0, "Umounting %s", URL );

	plugin = findNetworkPlugin ( service->network );
	if ( !plugin )
	{
		LNCdebug ( LNC_ERROR, 1, "umount() -> findNetworkPlugin( %s )", service->network );
		return -EXIT_FAILURE;
	}

	retval = plugin->umount ( service );
	LNCdebug ( LNC_INFO, 0, "umount( %s ) = %d ( %s )", URL, retval, strerror_r ( retval * -1, errorString, BUFSIZ ) );
	putNetworkPlugin ( plugin );

	return retval;
}

static int getDomains ( Service *service )
{
	int retval;
	NetworkPlugin *plugin;
	u_int32_t timeStamp = getProgramTime();

	LNCdebug ( LNC_INFO, 0, "Looking up domains for protocol %s", service->network );

	plugin = findNetworkPlugin ( service->network );
	if ( !plugin )
	{
		LNCdebug ( LNC_ERROR, 1, "getDomains() -> findNetworkPlugin( %s )", service->network );
		return -EXIT_FAILURE;
	}

	if ( !plugin->getDomains )
		retval = pingScan ( NULL, 0, service->flags & FORCE_RESCAN ? 1 : 0 );
	else
	{
		retval = plugin->getDomains ( service );
		if ( retval != -EACCES )
			removeExpiredBrowselistEntries ( timeStamp, service, 0 );
	}

	putNetworkPlugin ( plugin );
	service->lastTimeUsed = timeStamp;

	return retval;
}

static int getHosts ( Service *service )
{
	int retval;
	NetworkPlugin *plugin;
	u_int32_t timeStamp = getProgramTime();

	LNCdebug ( LNC_INFO, 0, "Looking up hosts for domain %s", service->domain );

	plugin = findNetworkPlugin ( service->network );
	if ( !plugin )
	{
		LNCdebug ( LNC_ERROR, 1, "getHosts() -> findNetworkPlugin( %s )", service->network );
		return -EXIT_FAILURE;
	}

	if ( !plugin->getDomains )
		retval = pingScan ( NULL, 0, service->flags & FORCE_RESCAN ? 1 : 0 );
	else
	{
		retval = plugin->getHosts ( service );
		if ( retval != -EACCES )
			removeExpiredBrowselistEntries ( timeStamp, service, 0 );
	}

	putNetworkPlugin ( plugin );
	service->lastTimeUsed = timeStamp;

	return retval;
}

static int getShares ( Service *service, struct auth *auth )
{
	int retval;
	NetworkPlugin *plugin;
	u_int32_t timeStamp = getProgramTime();

	LNCdebug ( LNC_INFO, 0, "Looking up shares for host %s, username = \"%s\", password = \"%s\"", service->host, auth->username, auth->password );

	plugin = findNetworkPlugin ( service->network );
	if ( !plugin )
	{
		LNCdebug ( LNC_ERROR, 1, "getShares() -> findNetworkPlugin( %s )", service->network );
		return -EXIT_FAILURE;
	}

	retval = plugin->getShares ( service, auth );
	if ( retval != EACCES )
		removeExpiredBrowselistEntries ( timeStamp, service, 0 );

	putNetworkPlugin ( plugin );
	service->lastTimeUsed = timeStamp;

	printList( browselist );

	return retval;
}

pthread_mutex_t pingMutex = PTHREAD_MUTEX_INITIALIZER;

static int pingScan ( const char *hostname, int port, int forceRescan )
{
	char exec[ PATH_MAX ];
	char *output;
	char *ptr;
	char *host;
	char *domain;
	char URL[ PATH_MAX ];
	char tmp[ NAME_MAX + 1 ];
	char hostBuffer[ NAME_MAX + 1 ];
	char domainBuffer[ NAME_MAX + 1 ];
	char portNumber[ 5 ];
	char portNumberList[ BUFSIZ ] = "";
	int exitStatus;
	int fullScan;
	u_int32_t browselistUpdateInterval;
	static u_int32_t previousTime = 0;
	u_int32_t currentTime;
	u_int32_t IP;
	Service *new, *found;
	NetworkPlugin *plugin;

	if ( !hostname || !port )
		fullScan = 1;
	else
		fullScan = 0;

	if ( fullScan )
	{
		pthread_mutex_lock ( &pingMutex );
		lockLNCConfig ( READ_LOCK );
		browselistUpdateInterval = networkConfig.BrowselistUpdateInterval;
		unlockLNCConfig();

		currentTime = getProgramTime();
		if ( ( currentTime >= previousTime ) || forceRescan )
			previousTime = currentTime + ( browselistUpdateInterval * 100 );
		else
		{
			pthread_mutex_unlock ( &pingMutex );
			return EXIT_SUCCESS;
		}


		lockList ( networkPluginList, READ_LOCK );
		forEachListEntry ( networkPluginList, plugin )
		{
			if ( plugin->portNumber )
			{
				snprintf ( portNumber, 5, "%d", plugin->portNumber );
				strcat ( portNumberList, portNumber );
				strcat ( portNumberList, "," );
			}
		}
		unlockList ( networkPluginList );

		if ( portNumberList[ 0 ] == '\0' )
		{
			LNCdebug ( LNC_INFO, 0, "pingScan(): No registered ports to scan" );
			return EXIT_SUCCESS;
		}
	}
	else
	{
		if ( !isValidIP ( hostname ) )
		{
			IP = getIPAddress ( hostname );
			if ( IP == 0 )
			{
				LNCdebug ( LNC_INFO, 1, "pingScan( %s, %d, %d ) -> getIPAddress( %s )", hostname, port, forceRescan, hostname );
				return -ENOMSG;
			}
		}

	}

	if ( !fullScan )
		snprintf ( exec, PATH_MAX, "netscan --network %s --protocol %d -s", hostname, port );
	else
	{
		lockLNCConfig ( READ_LOCK );
		if ( networkConfig.UseBroadcast )
			LNCdebug ( LNC_INFO, 0, "pingScan(): Using broadcast" );
		snprintf ( exec, PATH_MAX, "netscan --network %s --protocol %s %s %s %s", networkConfig.PingNetworks, portNumberList, networkConfig.UseBroadcast ? "--broadcast" : "", networkConfig.IgnoreAddresses ? "--ignore" : "", networkConfig.IgnoreAddresses ? networkConfig.IgnoreAddresses : "" );
		unlockLNCConfig();
	}

	output = execute ( exec, STDOUT_FILENO | STDERR_FILENO, environ, &exitStatus );
	if ( !output )
	{
		LNCdebug ( LNC_ERROR, 1, "pingScan() -> execute( %s )", exec );
		if ( fullScan )
		{
			previousTime = 0;
			pthread_mutex_unlock ( &pingMutex );
		}
		return -ENOMSG;
	}

	if ( exitStatus != 0 )
	{
		LNCdebug ( LNC_ERROR, 0, "pingScan() -> execute( %s ): output:\n%s", exec, output );
		if ( fullScan )
		{
			previousTime = 0;
			pthread_mutex_unlock ( &pingMutex );
		}
		free ( output );
		return -ENOMSG;
	}

	ptr = output;
	while ( ptr )
	{
		tmp[ 0 ] = '\0';
		sscanf ( ptr, "%s\t%s", tmp, portNumber );
		ptr = strchr ( ptr, '\n' );
		if ( ptr )
			++ptr;

		if ( strcmp ( tmp, "" ) == 0 )
			continue;

		IP = IPtoNumber ( tmp );
		if ( IP == 0 )
			continue;

		if ( hostname && !isValidIP ( hostname ) )
		{
			if ( isInternetDomain ( hostname ) )
			{
				host = ( char * ) hostname;
				domain = NULL;
			}
			else
			{
				snprintf ( tmp, NAME_MAX+1, "%s", hostname );
				host = tmp;
				domain = NULL;
				ptr = strchr ( tmp, '.' );
				if ( ptr )
				{
					*ptr = '\0';
					if ( * ( ptr+1 ) != '\0' )
						domain = ptr+1;
				}

			}
		}
		else
		{
			domain = getDomainName ( IP, domainBuffer, NAME_MAX + 1 );
			host = getHostName ( IP, hostBuffer, NAME_MAX + 1, SHORT_HOSTNAME );
		}

		lockList ( networkPluginList, READ_LOCK );
		forEachListEntry ( networkPluginList, plugin )
		{
			if ( plugin->portNumber == atoi ( portNumber ) )
			{

				if ( domain )
				{
					LNCdebug ( LNC_INFO, 0, "pingScan( %s, %d, %s ): Adding domain %s to network %s", hostname ? hostname : "", port, forceRescan ? "FORCE_RESCAN" : "", domain, plugin->protocol );

					snprintf ( URL, PATH_MAX, "network://%s/%s", plugin->protocol, domain );
					lockList( browselist, WRITE_LOCK );
					found = findListEntryUnlocked( browselist, URL );
					if ( !found ) {
						new = newServiceEntry ( 0, strdup ( domain ), NULL, NULL, NULL, strdup ( plugin->protocol ) );
						if ( !fullScan )
							new->hostType = MANUAL_HOST;
						addListEntryUnlocked( browselist, new );
					} else {
						updateServiceTimeStamp( found );
						putListEntry( browselist, found );
					}
					unlockList( browselist );
				}

				LNCdebug ( LNC_INFO, 0, "pingScan( %s, %d, %s ): Adding host %s in domain %s to network %s", hostname ? hostname : "", port, forceRescan ? "FORCE_RESCAN" : "", host, domain, plugin->protocol );

				if ( domain )
					snprintf ( URL, PATH_MAX, "network://%s/%s/%s", plugin->protocol, domain, host );
				else
					snprintf ( URL, PATH_MAX, "network://%s/%s", plugin->protocol, host );

				lockList( browselist, WRITE_LOCK );
				found = findListEntryUnlocked( browselist, URL );
				if ( !found ) {
					new = newServiceEntry ( IP, domain ? strdup ( domain ) : NULL, strdup ( host ), NULL, NULL, strdup ( plugin->protocol ) );
					if ( !fullScan )
						new->hostType = MANUAL_HOST;
					addListEntryUnlocked ( browselist, new );
				} else {
					updateServiceTimeStamp( found );
					putListEntry( browselist, found );
				}
				unlockList( browselist );
			}
		}
		unlockList ( networkPluginList );
	}


	free ( output );
	if ( !fullScan ) {
		printList( browselist );
		return EXIT_SUCCESS;
	}

	lockList ( networkPluginList, READ_LOCK );
	forEachListEntry ( networkPluginList, plugin )
	{
		if ( !plugin->portNumber )
			continue;

		snprintf ( URL, PATH_MAX, "network://%s", plugin->protocol );
		found = findListEntry ( browselist, URL );
		if ( !found )
			continue;

		removeExpiredBrowselistEntries ( currentTime, found, 1 );
		putListEntry ( browselist, found );
	}
	unlockList ( networkPluginList );

	pthread_mutex_unlock ( &pingMutex );

	printList( browselist );

	return EXIT_SUCCESS;
}

void markAsBadURL ( const char *URL )
{
	Service * service;
	char network[ PATH_MAX ];
	char domain[ PATH_MAX ];
	char host[ PATH_MAX ];
	char share[ PATH_MAX ];
	int depth;
	char *search;

	LNCdebug ( LNC_INFO, 0, "markAsBadURL( %s ): Marking URL as bad connection", URL );
	network[ 0 ] = host[ 0 ] = domain[ 0 ] = share[ 0 ] = '\0';
	depth = breakPendingService ( URL, network, domain, host, share );
	if ( depth < 3 )
		return ;

	search = ( depth == 3 ) ? domain : ( ( depth == 4 ) ? host : share );
	service = findListEntry ( browselist, search );
	if ( !service )
	{
		LNCdebug ( LNC_ERROR, 1, "markAsBadURL( %s ) -> findListEntry( browselist, %s )", URL, search );
		return ;
	}

	setBusy ( service );
	service->badConnection = 1;
	unsetBusy ( service );
	putListEntry ( browselist, service );
}

pthread_once_t browselistInit = PTHREAD_ONCE_INIT;

void initBrowselist ( void )
{
	struct passwd * user;
	char buffer[ BUFSIZ ];
	int retval;

	user = getSessionInfoByUID ( geteuid(), buffer );
	if ( !user )
	{
		LNCdebug ( LNC_ERROR, 1, "initBrowselist() -> getSessionInfoByUID()" );
		return ;
	}

	sprintf ( realRoot, "%s/%d", LNC_ROOT_DIR, getpid() );
	realRootLength = strlen ( realRoot );
	retval = buildPath ( realRoot, 0755 );
	if ( retval < 0 )
	{
		LNCdebug ( LNC_ERROR, 1, "initBrowselist() -> buildPath( %s )", realRoot );
		return ;
	}


	browselist = initList ( browselist, netCompare, netFind, netFree, netPrint, netonInsert, netonUpdate, netonRemove );
}

void cleanupBrowselist ( void )
{
	int retval;

	LNCdebug ( LNC_DEBUG, 0, "cleanupBrowselist()" );

	stopServiceTimer();
	clearListReverse ( browselist );
	retval = rmdir ( realRoot );
	if ( retval < 0 )
		LNCdebug ( LNC_ERROR, 1, "cleanupBrowselist() -> rmdir( %s )", realRoot );
}

int fillin ( const char *URL, char *protocol, char *domain, char *host, char *IP, char *share )
{
	char *ptr;
	char *token;
	char *tmp;
	char buffer[ PATH_MAX ];
	int depth;

	if ( protocol )
		protocol[ 0 ] = '\0';

	if ( domain )
		domain[ 0 ] = '\0';

	if ( host )
		host[ 0 ] = '\0';

	if ( IP )
		IP[ 0 ] = '\0';

	if ( share )
		share[ 0 ] = '\0';

	snprintf ( buffer, PATH_MAX, "%s", URL );
	ptr = buffer;
	for ( depth = 0; ( token = strtok_r ( ptr, "/", &tmp ) ); ++depth )
	{
		ptr = NULL;
		if ( depth == 0 )
			continue;
		else if ( depth == 1 )
		{
			if ( !protocol )
				continue;

			strlcpy ( protocol, token, NAME_MAX + 1 );
		}
		else if ( depth == 2 )
		{
			if ( !domain )
				continue;

			strlcpy ( domain, token, NAME_MAX + 1 );
		}
		else if ( depth == 3 )
		{
			if ( host )
				strlcpy ( host, token, NAME_MAX + 1 );

			if ( IP )
				strlcpy ( IP, token, NAME_MAX + 1 );
		}
		else if ( depth == 4 )
		{
			if ( !share )
				continue;
			strlcpy ( share, token, NAME_MAX + 1 );
		}
		else
			break;
	}


	return EXIT_SUCCESS;
}

int scanURL ( const char *URL, unsigned int flags, int *usePluginSyscall )
{
	char networkSearch[ PATH_MAX ];
	char domainSearch[ PATH_MAX ];
	char hostSearch[ PATH_MAX ];
	char shareSearch[ PATH_MAX ];
	char *search;
	char *ptr;
	int result;
	int depth, i;
	int flag;
	int rescan;
	int noGetShares;
	int port;
	int pluginType;
	struct auth auth;
	u_int32_t timeStamp;
	NetworkPlugin *plugin;
	Service * service;

	*usePluginSyscall = 0;

	networkSearch[ 0 ] = hostSearch[ 0 ] = shareSearch[ 0 ] = '\0';
	depth = breakPendingService ( URL, networkSearch, domainSearch, hostSearch, shareSearch );
	if ( depth < 2 )
		return -ENODEV;

	plugin = findNetworkPlugin ( ( char * ) URL+10 );
	if ( !plugin )
	{
		LNCdebug ( LNC_ERROR, 1, "scanURL( %s, %d ) -> findNetworkPlugin( %s )", URL, flags, URL+10 );
		return -EXIT_FAILURE;
	}

	pluginType = plugin->type;

	if ( plugin->getShares )
		noGetShares = 0;
	else
		noGetShares =  1;

	port = plugin->portNumber;
	putNetworkPlugin ( plugin );

	if ( depth == ( 6 - noGetShares ) )
	{
		depth -= 1;
		flags &= ~PARTIAL_RESOLVE;
	}

//	flags |= getConnectionParameters();

	for ( i = 2; i <= depth; ++i )
	{
		search = ( i == 2 ) ? networkSearch : ( ( i == 3 ) ? domainSearch : ( ( i == 4 ) ? hostSearch : shareSearch ) );
		service = findListEntry ( browselist, search );
		if ( !service && i == 3 )
		{
			if ( plugin->portNumber )
			{
				ptr = strrchr ( search, '/' );
				if ( ptr )
				{
					pingScan ( ptr+1, plugin->portNumber, 1 );
					service = findListEntry ( browselist, search );
				}
			}
		}

		if ( !service )
		{
			LNCdebug ( LNC_ERROR, 1, "scanURL( %s, %d ) -> findListEntry( browselist, %s )", URL, flags, search );
			return -errno;
		}
		else if ( service->badConnection != 0 )
		{
			LNCdebug ( LNC_ERROR, 0, "scanURL( %s, %d ) -> findListEntry( browselist, %s ): This service is not reliable", URL, flags, search );
			putListEntry ( browselist, service );
			return -ENODEV;
		}

		if ( pluginType == USE_BROWSELIST )
		{
			if ( service->share || ( service->host && noGetShares ) )
			{
				*usePluginSyscall = 1;
				putListEntry ( browselist, service );
				break;
			}
		}

		if ( ( flags & PARTIAL_RESOLVE ) && i == depth )
		{
			putListEntry ( browselist, service );
			continue;
		}

		flag = 0;
		setBusy ( service );
		service->flags = 0;
		if ( flags & DISABLE_AUTHENTICATION )
			flag |= DISABLE_AUTHENTICATION;

		if ( ( flags & FORCE_FULL_RESCAN ) || ( ( flags & FORCE_RESCAN ) && ( depth == i ) ) )
		{
			flag |= FORCE_RESCAN;
			service->flags |= FORCE_RESCAN;
			service->error = 0;
			if ( service->status == SERVICE_MOUNTED )
			{
				service->flags |= LAZY_UMOUNT;
				umount ( service );
			}
			service->status = NEEDS_ACTION;
		}

		if ( ( flags & FORCE_AUTHENTICATION ) && ( depth == i ) )
		{
			flag |= FORCE_AUTHENTICATION | FORCE_RESCAN;
			service->flags |= FORCE_AUTHENTICATION | FORCE_RESCAN;
			service->error = 0;
			if ( service->status == SERVICE_MOUNTED )
			{
				service->flags |= LAZY_UMOUNT;
				umount ( service );
			}
			service->status = NEEDS_ACTION;
		}

		if ( !needsAction ( service ) )
		{
			if ( service->share || ( service->host && noGetShares ) )
			{
				service->expireTime = getProgramTime();
				i = depth;
			}

			unsetBusy ( service );
			putListEntry ( browselist, service );
			continue;
		}

		if ( service->share || service->host )
			readAuthorizationInfo ( search, &auth );

		timeStamp = getProgramTime();
		rescan = 1;
		result = 0;
		if ( service->error == EACCES )
		{
			flag |= FORCE_AUTHENTICATION;
			service->flags |= FORCE_AUTHENTICATION;
			result = -service->error;
		}
		else if ( service->error && ! ( flag & FORCE_RESCAN ) && ( service->lastTimeUsed + 500 ) > timeStamp )
		{
			rescan = 0;
			result = -service->error;
		}

		do
		{
			int saveAuthorization = 0;

			if ( flag & FORCE_AUTHENTICATION )
			{
				if ( ( ( flag & DISABLE_AUTHENTICATION ) ? -1 : requestAuthorizationDialog ( search, &auth ) ) < 0 )
				{
					rescan = 0;
					result = -EACCES;
				}
				else
				{
					rescan = 1;
					saveAuthorization = 1;
				}
			}

			if ( rescan )
			{
				if ( service->share || ( service->host && noGetShares ) )
					result = mount ( service, &auth );
				else if ( service->host )
					result = getShares ( service, &auth );
				else if ( service->domain )
					result = getHosts ( service );
				else
					result = getDomains ( service );

				if ( result == -EACCES )
				{
					service->flags |= FORCE_AUTHENTICATION;
					flag |= FORCE_AUTHENTICATION;
				}
				else if ( result != EACCES && saveAuthorization && ( service->share || service->host ) )
					writeAuthorizationInfo ( search, &auth );
			}
		}
		while ( result == -EACCES && rescan );

		if ( rescan == 0 && i > 2 )
			LNCdebug ( LNC_INFO, 0, "scanURL( %s ): %s, returning cached error %d", URL, search, result );
		else if ( rescan )
			LNCdebug ( LNC_INFO, 0, "scanURL( %s ): %s, returning %d\tservice->expireTime = \"%u\"", URL, search, result, service->expireTime );

		if ( result == 0 )
		{
			service->error = 0;
			if ( service->share || ( service->host && noGetShares ) )
				service->status = SERVICE_MOUNTED;
			else
				service->status = SCANNED;

			service->expireTime = getProgramTime();
		}
		else
		{
			service->error = result * -1;
			service->status = NEEDS_ACTION;
		}

		if ( service->share || ( service->host && noGetShares ) )
			i = depth;

		unsetBusy ( service );
		putListEntry ( browselist, service );

		if ( result == 0 )
			restartServiceTimer();

		if ( result < 0 && service->host /*i > 3*/ )
		{
			if ( result == -ENODEV )
				markAsBadURL ( URL );
			return result;
		}

	}

	return EXIT_SUCCESS;
}

pthread_mutex_t timerMutex = PTHREAD_MUTEX_INITIALIZER;
int timerIsRunning = 0;
int restartTimer = 0;
pthread_t timerTID = 0;

static void serviceTimer ( void )
{
	int retval, used, keepRunning;
	char path[ PATH_MAX ];
	char buffer[ PATH_MAX ];
	Service *service;
	NetworkPlugin *plugin;
	u_int32_t currentTime;
	u_int32_t firstExpireTime;
	u_int32_t expireTime;
	u_int32_t totalSleepTime;
	float tmp;

	timerTID = pthread_self();

	do
	{
		currentTime = getProgramTime();
		firstExpireTime = 0;
		keepRunning = 0;

		lockLNCConfig ( READ_LOCK );
		lockList ( browselist, READ_LOCK );
		forEachListEntryReverse ( browselist, service )
		{

			if ( service->status <= NEEDS_ACTION )
				continue;

			plugin = findNetworkPlugin ( service->network );
			if ( !plugin )
			{
				LNCdebug ( LNC_ERROR, 1, "serviceTimer() -> findNetworkPlugin( %s )", service->network );
				continue;
			}

			if ( service->share || ( !plugin->getShares && service->host ) )
				expireTime = service->expireTime + ( networkConfig.UmountInterval * 100 );
			else
				expireTime = service->expireTime + ( networkConfig.BrowselistUpdateInterval * 100 );

			putNetworkPlugin ( plugin );

			LNCdebug ( LNC_DEBUG, 0, "%s expires at %u", createServiceKey ( buffer, PATH_MAX, service, 1 ), expireTime );
			if ( expireTime > currentTime )
			{
				if ( ( firstExpireTime == 0 ) || ( expireTime < firstExpireTime ) )
				{
					LNCdebug ( LNC_DEBUG, 0, "Setting firstExpireTime to %u", expireTime );
					firstExpireTime = expireTime;
				}

				keepRunning = 1;
				if ( expireTime > currentTime )
				{
//					keepRunning = 1;
					continue;
				}
			}

			if ( tryToSetBusy ( service ) )
			{
				if ( service->status == SCANNED )
				{
					LNCdebug ( LNC_INFO, 0, "EXPIRED: resetting service = %s, expire time = %u, current time = %u", createServiceKey ( buffer, PATH_MAX, service, 1 ), service->expireTime, currentTime );
					service->badConnection = 0;
					service->error = 0;
					service->status = NEEDS_ACTION;
					unsetBusy ( service );
					continue;
				}

				createServicePath ( path, PATH_MAX, service );
				used = isMountInUse ( path );
				if ( used <= 0 )
				{
					if ( used < 0 )
						LNCdebug ( LNC_ERROR, 1, "serviceTimer() -> isMountInUse( %s )", path );
					LNCdebug ( LNC_INFO, 0, "NOT USED: unmounting service = %s, expire time = %u, current time = %u", createServiceKey ( buffer, PATH_MAX, service, 1 ), service->expireTime, currentTime );
					retval = umount ( service );
					if ( retval < 0 )
					{
						service->expireTime = getProgramTime();
						expireTime = service->expireTime + ( networkConfig.UmountInterval * 100 );
						if ( ( firstExpireTime == 0 ) || ( expireTime < firstExpireTime ) )
							firstExpireTime = expireTime;
						keepRunning = 1;
					}
					else
					{
						service->badConnection = 0;
						service->error = 0;
						service->status = NEEDS_ACTION;
					}
				}
				else
				{
					service->expireTime = getProgramTime();
					expireTime = service->expireTime + ( networkConfig.UmountInterval * 100 );
					if ( ( firstExpireTime == 0 ) || ( expireTime < firstExpireTime ) )
						firstExpireTime = expireTime;
					keepRunning = 1;
				}
				unsetBusy ( service );
			}
			else
				keepRunning = 1;
		}
		unlockList ( browselist );
		unlockLNCConfig();

		if ( keepRunning && timerIsRunning )
		{
			if ( firstExpireTime < ( currentTime + 100 ) )
				firstExpireTime = currentTime + 100;

			tmp = ( firstExpireTime - currentTime ) / 100;
			LNCdebug ( LNC_INFO, 0, "Sleeping for \"%.2f\" seconds %d, %d", tmp, firstExpireTime, currentTime );
			for ( totalSleepTime = firstExpireTime - currentTime; totalSleepTime; totalSleepTime -= 100 )
			{
				usleep ( 1000000 );
				if ( totalSleepTime < 100 )
					totalSleepTime = 100;

				if ( restartTimer )
				{
					LNCdebug ( LNC_DEBUG, 0, "serviceTimer(): restarting" );
					restartTimer = 0;
					break;
				}
			}
		}
	}
	while ( keepRunning && timerIsRunning );

	if ( !keepRunning )
		LNCdebug ( LNC_INFO, 0, "No more services left, stoping service unmounter" );
	pthread_mutex_lock ( &timerMutex );
	timerIsRunning = 0;
	timerTID = 0;
	pthread_mutex_unlock ( &timerMutex );
}

void startServiceTimer ( void )
{
	void * retval;

	if ( !browselistInitialized )
		return;

	lockLNCConfig ( READ_LOCK );
	if ( networkConfig.EnableDebugMode )
	{
		unlockLNCConfig();
		return ;
	}
	unlockLNCConfig();

	pthread_mutex_lock ( &timerMutex );
	if ( !timerIsRunning )
	{
		LNCdebug ( LNC_INFO, 0, "Starting service timer" );
		timerIsRunning = 1;
		retval = createThread ( ( void * ( * ) ( void * ) ) serviceTimer, NULL, DETACHED );
		if ( !retval )
			LNCdebug ( LNC_ERROR, 1, "startServiceUMounter() -> createThread(serviceUMounter)" );
	}
	pthread_mutex_unlock ( &timerMutex );
}

void restartServiceTimer ( void )
{
	void * retval;

	if ( !browselistInitialized )
		return;

	lockLNCConfig ( READ_LOCK );
	if ( networkConfig.EnableDebugMode )
	{
		unlockLNCConfig();
		return ;
	}
	unlockLNCConfig();

	LNCdebug ( LNC_INFO, 0, "Restarting service timer" );
	pthread_mutex_lock ( &timerMutex );
	if ( !timerIsRunning )
	{
		timerIsRunning = 1;
		retval = createThread ( ( void * ( * ) ( void * ) ) serviceTimer, NULL, DETACHED );
		if ( !retval )
			LNCdebug ( LNC_ERROR, 1, "startServiceUMounter() -> createThread(serviceUMounter)" );
	}
	else if ( timerTID )
		restartTimer = 1;

	pthread_mutex_unlock ( &timerMutex );
}

void stopServiceTimer ( void )
{
	lockLNCConfig ( READ_LOCK );
	if ( networkConfig.EnableDebugMode )
	{
		unlockLNCConfig();
		return ;
	}
	unlockLNCConfig();

	LNCdebug ( LNC_INFO, 0, "Stopping service timer" );
	pthread_mutex_lock ( &timerMutex );
	timerIsRunning = 0;
	if ( timerTID )
		restartTimer = 1;
	pthread_mutex_unlock ( &timerMutex );
}

int isValidIP ( const char *IP )
{
	char *ptr;
	int i;

	errno = EFAULT;

	if ( *IP == '.' )
		return 0;

	if ( * ( IP+strlen ( IP ) - 1 ) == '.' )
		return 0;

	if ( strlen ( IP ) < 7 ||  strlen ( IP ) > 15 )
		return 0;

	ptr = ( char * ) IP;
	for ( i = 0; ( ptr = strchr ( ptr, '.' ) ); ++i )
		++ptr;

	if ( i != 3 )
		return 0;

	if ( !IPtoNumber ( IP ) )
		return 0;

	errno = 0;

	return 1;
}
/*
int updateManualHosts ( const char * path, const char * filename, int event )
{
	char *URL;
	char search[ PATH_MAX ];
	char *tmp;
	char *domain;
	char *host;
	char *token;
//	char *IP;
	char *ptr;
	int depth;
	struct key *key;
	Service *new, *found;
	NetworkPlugin *plugin;


	LNCdebug ( LNC_DEBUG, 0, "updateManualHosts(): path = %s, filename = %s, event = %s", path, filename, printFileMonitorEvent ( event ) );
	if ( event != FAMCreated && event != FAMChanged )
		return -EXIT_FAILURE;

	initKeys ( "LNC/hosts" );
	rewindKeys ( "LNC/hosts", USER_ROOT );
	while ( ( key = forEveryKey2 ( "LNC/hosts", USER_ROOT ) ) )
	{
		if ( key->type != LNC_CONFIG_KEY )
		{
			free ( key );
			continue;
		}

//				snprintf ( URL, PATH_MAX, "%s", key->name );
//				IP = getCharKey ( "LNC/hosts", URL, NULL );
//				if ( !IP )
//				{
//					LNCdebug ( LNC_ERROR, 1, "updateManualHosts( %s, %s, %d ) -> getCharKey( %s )", path, filename, event, URL );
//					free ( IP );
//					free ( key );
//					continue;
//				}
//
//				if ( !isValidIP ( IP ) )
//				{
//					LNCdebug ( LNC_ERROR, 1, "updateManualHosts( %s, %s, %d ) -> isValidIP( %s )", path, filename, event, IP );
//					free ( IP );
//					free ( key );
//					continue;
//				}
//
//				replaceCharacter ( URL, '\\', '/' );
		URL = getCharKey ( "LNC/hosts", key->name, NULL );
		if ( !URL )
		{
			LNCdebug ( LNC_ERROR, 1, "updateManualHosts( %s, %s, %s ) -> getCharKey( %s )", path, filename, printFileMonitorEvent ( event ), key->name );
			free ( key );
			continue;
		}

		depth = getDepth ( URL );
		if ( depth != 4 )
		{
			LNCdebug ( LNC_ERROR, 0, "updateManualHosts( %s, %s, %s ) -> getDepth( %s ) = %d: Invalid host format, skipping", path, filename, printFileMonitorEvent ( event ), URL, depth );
			free ( URL );
			free ( key );
			continue;
		}

		plugin = findNetworkPlugin ( URL+10 );
		if ( !plugin )
		{
			LNCdebug ( LNC_ERROR, 0, "updateManualHosts( %s, %s, %s ) -> findNetworkPlugin(): Unsupported network plugin", path, filename, printFileMonitorEvent ( event ) );
			free ( URL );
			free ( key );
			continue;
		}

		snprintf ( search, PATH_MAX, "%s", URL );
		ptr = URL;
		for ( depth = 1; ( token = strtok_r ( ptr, "/", &tmp ) ); ++depth )
		{
			ptr = NULL;
			if ( depth == 1 || depth == 2 )
				continue;
			else if ( depth == 3 )
			{
				ptr = strstr ( search, token );
				if ( !ptr || * ( ptr+1 ) == '\0' || ! ( ptr = strchr ( ptr+1, '/' ) ) )
					break;

				*ptr = '\0';
				found = findListEntry ( browselist, search );
				if ( found )
				{
					found->flags |= MANUAL_HOST;
					if ( !found->name )
						found->name = strdup ( key->name );
					putListEntry ( browselist, found );
				}
				else
				{
					printf ( "adding %s to browslist\n", token );
					domain = strdup ( token );
					new = newServiceEntry ( 0, domain, NULL, NULL, NULL, strdup ( plugin->protocol ) );
					new->flags |= MANUAL_HOST;
					new->name = strdup ( key->name );
					addListEntry ( browselist, new );
				}

				*ptr = '/';
				ptr = NULL;
			}
			else if ( depth == 4 )
			{
				found = findListEntry ( browselist, search );
				if ( found )
				{
					found->flags |= MANUAL_HOST;
					if ( !found->name )
						found->name = strdup ( key->name );
					putListEntry ( browselist, found );
				}
				else
				{
					printf ( "adding %s in %s to browslist\n", token, domain );
					host = strdup ( token );
					new = newServiceEntry ( 0, strdup ( domain ), host, NULL, NULL, strdup ( plugin->protocol ) );
					new->flags |= MANUAL_HOST;
					new->name = strdup ( key->name );
					addListEntry ( browselist, new );

				}

			}

		}

		free ( URL );
		free ( key );
		putNetworkPlugin ( plugin );
	}

	cleanupKeys ( "LNC/hosts" );

	return EXIT_SUCCESS;
}*/

/*static int snpCompare( struct simplenetplugin *new, struct simplenetplugin *entry ) {
	return strcmp( new->protocol, entry->protocol );
}

static int snpFind( char *search, struct simplenetplugin *entry ) {
	return strcmp( search, entry->protocol );
}
static void snpPrint( struct simplenetplugin *entry ) {
	LNCdebug( LNC_INFO, 0, "%s", entry->protocol );
}

atomic_t snpListInit = { 1 };

int registerSimpleNetworkPlugin( const char *protocol, const char *networkName, SNPAuthRoutine mount, SNPRoutine umount, SNPRoutine getDomains, SNPRoutine getHosts, SNPAuthRoutine getShares ) {
	struct simplenetplugin * plugin;
	Service * service;
	struct passwd *user;
	int retval;

	LNCdebug( LNC_INFO, 0, "registerSimpleNetworkPlugin(): registering protocol %s", protocol );

	if ( atomic_dec_and_test( &snpListInit ) ) {
		user = getSessionInfoByUID( geteuid() );
		if ( !user ) {
			LNCdebug( LNC_ERROR, 1, "registerSimpleNetworkPlugin() -> getSessionInfoByUID()" );
			return -EXIT_FAILURE;
		}

		sprintf( realRoot, "%s/tmp/network/%d", user->pw_dir, getpid() );
		realRootLength = strlen( realRoot );
		retval = buildPath( realRoot, 0755 );
		if ( retval < 0 ) {
			LNCdebug( LNC_ERROR, 1, "registerSimpleNetworkPlugin() -> buildPath( %s )", realRoot );
			return -EXIT_FAILURE;
		}

		browselist = initList( browselist, netCompare, netFind, netFree, netPrint, netonInsert, netonUpdate, netonRemove );
		simpleNetworkPluginList = initList( simpleNetworkPluginList, snpCompare, snpFind, NULL, snpPrint, NULL, NULL );
	}

	plugin = malloc( sizeof( struct simplenetplugin ) );
	if ( !plugin ) {
		LNCdebug( LNC_ERROR, 1, "registerSimpleNetworkPlugin( %s ) -> malloc()", protocol );
		return -EXIT_FAILURE;
	}

	strcpy( plugin->protocol, protocol );
	strcpy( plugin->networkName, networkName );
	plugin->mount = mount;
	plugin->umount = umount;
	plugin->getDomains = getDomains;
	plugin->getHosts = getHosts;
	plugin->getShares = getShares;

	addListEntry( simpleNetworkPluginList, plugin );

	service = newServiceEntry( 0, NULL, NULL, NULL, strdup( networkName ), strdup( protocol ) );
	addListEntry( browselist, service );

	return 0;
}

int unregisterSimpleNetworkPlugin( const char *protocol ) {
	struct simplenetplugin * plugin;
	int retval;
	Service *service;

	LNCdebug( LNC_INFO, 0, "unregisterSimpleNetworkPlugin(): Cleaning up" );
	lockList( browselist, WRITE_LOCK );
	forEachListEntryReverse( browselist, service ) {
		if ( strcmp( service->network, protocol ) == 0 )
			delListEntryUnlocked( browselist, service );
	}

	plugin = findListEntry( simpleNetworkPluginList, ( void * ) protocol );
	if ( plugin ) {
		delListEntry( simpleNetworkPluginList, plugin );
		putListEntry( simpleNetworkPluginList, plugin );
	}

	unlockList( browselist );

	retval = rmdir( realRoot );
	if ( retval < 0 )
		LNCdebug( LNC_ERROR, 1, "unregisterSimpleNetworkPlugin( %s ) -> rmdir( %s )", protocol, realRoot );

	if ( isListEmpty( simpleNetworkPluginList ) )
		stopServiceTimer();

	return 0;
}*/
