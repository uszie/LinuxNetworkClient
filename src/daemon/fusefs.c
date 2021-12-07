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
#include "fusefs.h"
#include "session.h"
#include "networkclient.h"
#include "daemon.h"
#include "display.h"
#include "dir.h"
#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <sys/stat.h>
#include <sys/statfs.h>
#include <errno.h>
#include <dirent.h>
#include <libintl.h>

char homeDirectory[ PATH_MAX ];
//struct stat serviceAttributes;

NetworkPlugin *getProtocolPluginByPath( const char *path, int resolveShortcuts );
NetworkPlugin *getProtocolPluginByURL( const char *path, int resolveShortcuts );
int isShortcut( const char *URL );
static int getPathDepth( const char *path );
static struct application *lookupEnabledLibrary( pid_t PID );

int isShortcut( const char *URL ) {
	if ( strncmp( URL, "network://Shortcut", 18 ) == 0 )
		return 1;

	return 0;
}

static int getPathDepth( const char *path ) {
	char * cpy;
	char *ptr;
	char *token;
	char *previousToken;
	int i;
	unsigned int len;

	i = 0;
	cpy = ( char * ) path;
	previousToken = ( char * ) path;

	while ( ( token = strtok_r( cpy, "/", &ptr ) ) ) {
		len = token - previousToken;
		if ( len != 0 )
			previousToken[ len - 1 ] = '/';
		previousToken = token;
		cpy = NULL;
		i += 1;
		if ( i > 5 ) {
			if ( ptr && *ptr != '\0' ) {
				*( ptr - 1 ) = '/';
			}
			break;
		}
	}

	return i;
}

NetworkPlugin *getProtocolPluginByPath( const char *path, int resolveShortcuts ) {
	char protocol[ NAME_MAX + 1 ];
	char *ptr;
	char *begin;
	char *end;
	NetworkPlugin * plugin;

	strlcpy( protocol, path + 1, NAME_MAX + 1 );
	ptr = strchr( protocol, '/' );
	if ( ptr )
		* ptr = '\0';

	ptr = protocol;
	if ( resolveShortcuts ) {
		begin = strchr( protocol, '(' );
		if ( begin ) {
			end = strchr( begin+1, ')' );
			if ( end ) {
				*end = '\0';
				ptr = begin+1;
			}
		}
	}


	plugin = findNetworkPlugin( ptr );
	if ( plugin )
		return plugin;

	return NULL;
}

NetworkPlugin *getProtocolPluginByURL( const char *URL, int resolveShortcuts ) {
	char protocol[ NAME_MAX + 1 ];
	char *ptr;
	char *begin;
	char *end;
	NetworkPlugin * plugin;

	if ( strncmp( URL, "network://", 10 ) != 0 )
		return NULL;

	protocol[ 0 ] = '\0';
	if ( resolveShortcuts && ( strncmp( URL+10, "Shortcut", 8 ) == 0 ) ) {
		begin = strchr( URL+18, '(' );
		if ( begin ) {
			strlcpy( protocol, begin+1, NAME_MAX + 1 );
			end = strchr( protocol, ')' );
			if ( end )
				*end = '\0';
		}
	} else
		strlcpy( protocol, URL + 10, NAME_MAX + 1 );

	if ( protocol[ 0 ] == '\0' )
		return NULL;

	ptr = strchr( protocol, '/' );
	if ( ptr )
		* ptr = '\0';

	plugin = findNetworkPlugin( protocol );
	if ( plugin )
		return plugin;

	return NULL;

}

static char *buildURL( char *buf, const char *path, unsigned int *flags ) {
	char cpy[ PATH_MAX ];
	char *ptr;
	char *lastSlash;
	NetworkPlugin *plugin;

	strlcpy( cpy, path, PATH_MAX );

	lastSlash = strrchr( cpy, ' ' );
	if ( lastSlash && strcmp( lastSlash, " FORCE_AUTHENTICATION" ) == 0 ) {
		*lastSlash = '\0';
		if ( flags )
			* flags |= FORCE_RESCAN | FORCE_AUTHENTICATION;
	}

	ptr = ( char * ) cpy;
	if ( strcmp( cpy, "/" ) == 0 ) {
		strcpy( buf, "network://" );
		return buf;
	}

	plugin = getProtocolPluginByPath( ptr, 0 );
	if ( plugin ) {
		strcpy( buf, "network://" );
		strcpy( buf+10, plugin->protocol );
		ptr = strchr( ptr+1, '/' );
		if ( ptr )
			strcpy( buf+( 10+plugin->protocolLength ), ptr );

		putNetworkPlugin( plugin );
	} else if ( ( plugin = getProtocolPluginByPath( ptr, 1 ) ) ){
		strcpy( buf, "network://Shortcut/" );
		strlcpy( buf+19, ptr+1, PATH_MAX - 19 );
		putNetworkPlugin( plugin );
	} else {
		strcpy( buf, "network://" );
		strlcpy( buf+10, ptr+1, PATH_MAX - 10 );
	}

	return buf;
}

static pthread_key_t contextKey = 0;

static struct syscallContext *setSyscallContext( int syscall ) {
	struct syscallContext * syscallContext;
	struct fuse_context *fuseContext;
	char *name;
	u_int64_t currentTime = getCurrentTime();

	syscallContext = createTSDBuffer( sizeof( struct syscallContext ), &contextKey, 1 );
	fuseContext = fuse_get_context();
	syscallContext->PID = fuseContext->pid;
	syscallContext->UID = fuseContext->uid;
	syscallContext->GID = fuseContext->gid;
	name = getProcessName( syscallContext->PID );
	if ( !name )
		strcpy( syscallContext->applicationName, "Unknown" );
	else
		strcpy( syscallContext->applicationName, name );

	syscallContext->markForUpdate = 0;
	syscallContext->syscall = syscall;
	syscallContext->syscallID = random();
	syscallContext->application = findListEntry( applicationList, syscallContext->applicationName );
	if ( !syscallContext->application )
		syscallContext->application = lookupEnabledLibrary( syscallContext->PID );
	if ( syscallContext->application )
		syscallContext->application->syscallStartTime = currentTime;

	return syscallContext;
}

struct syscallContext *getSyscallContext( void ) {
	return createTSDBuffer( sizeof( struct syscallContext ), &contextKey, 0 );
}

void updateApplicationContext( struct syscallContext *context ) {
	u_int64_t currentTime;

	if ( !context )
		context = getSyscallContext();

	if ( !context || !context->application )
		return ;

	if ( context->application->disabled )
		goto out;

	if ( context->syscall == NETWORKCLIENT_STAT && context->markForUpdate == 0 ) {
		context->application->previousSyscall = context->syscall;
		goto out;
	}

	currentTime = getCurrentTime();
	if ( ( context->application->syscall & context->syscall ) && ( context->application->isLibrary ? 1 : hasProperArguments( context->PID, context->application ) ) ) {
		context->application->previousSyscallStartTime = context->application->syscallStartTime;
		context->application->previousSyscallEndTime = currentTime;
		context->application->previousSyscall = context->syscall;
	}

out:
	if ( context->application->isLibrary )
		putListEntry( libraryList, context->application );
	else
		putListEntry( applicationList, context->application );
}

struct application *lookupEnabledLibrary( pid_t PID ) {
	struct application *entry = NULL;
	char path[ PATH_MAX ];
	char *maps;
	int retval;

	sprintf( path, "/proc/%d/maps", PID );
	retval = copyFileToBuffer( &maps, path );
	if ( retval < 0 ) {
		LNCdebug( LNC_ERROR, 1, "lookupEnabledLibrary( %d ) -> copyFileToBuffer( %s )", PID, path );
		return NULL;
	}

	lockList( libraryList, READ_LOCK );
	forEachListEntry( libraryList, entry ) {
		if ( strstr( maps, entry->name ) ) {
			getListEntry( libraryList, entry );
			unlockList( libraryList );
			free( maps );
			return entry;
		}
	}
	unlockList( libraryList );
	free( maps );

	return NULL;
}

static char *URLArray[ 3 ] = { NULL, NULL, NULL };
static u_int64_t timeArray[ 3 ] = { 0, 0, 0 };

static void clearURLCache( int fullClear ) {
	if ( fullClear ) {
		if ( URLArray[ 0 ] )
			free( URLArray[ 0 ] );
		if ( URLArray[ 1 ] )
			free( URLArray[ 1 ] );
		if ( URLArray[ 2 ] )
			free( URLArray[ 2 ] );
		URLArray[ 0 ] = URLArray[ 1 ] = URLArray[ 2 ] = NULL;
		timeArray[ 0 ] = timeArray[ 1 ] = timeArray[ 2 ] = 0;
		return ;
	}

	if ( URLArray[ 2 ] ) {
		free( URLArray[ 0 ] );
		free( URLArray[ 1 ] );
		URLArray[ 0 ] = URLArray[ 2 ];
		URLArray[ 1 ] = URLArray[ 2 ] = NULL;
		timeArray[ 0 ] = timeArray[ 2 ];
		timeArray[ 1 ] = timeArray[ 2 ] = 0;
	} else if ( URLArray[ 1 ] ) {
		free( URLArray[ 0 ] );
		URLArray[ 0 ] = URLArray[ 1 ];
		URLArray[ 1 ] = URLArray[ 2 ] = NULL;
		timeArray[ 0 ] = timeArray[ 1 ];
		timeArray[ 1 ] = timeArray[ 2 ] = 0;
	}
}

static void addURL( const char *URL, u_int64_t time ) {
	if ( URLArray[ 0 ] == NULL ) {
		URLArray[ 0 ] = strdup( URL );
		timeArray[ 0 ] = time;
	} else if ( URLArray[ 1 ] == NULL ) {
		URLArray[ 1 ] = strdup( URL );
		timeArray[ 1 ] = time;
	} else if ( URLArray[ 2 ] == NULL ) {
		URLArray[ 2 ] = strdup( URL );
		timeArray[ 2 ] = time;
	} else {
		free( URLArray[ 0 ] );
		URLArray[ 0 ] = URLArray[ 1 ];
		URLArray[ 1 ] = URLArray[ 2 ];
		URLArray[ 2 ] = strdup( URL );
		timeArray [ 0 ] = timeArray[ 1 ];
		timeArray [ 1 ] = timeArray[ 2 ];
		timeArray [ 2 ] = time;
	}
}

static char *getLastURL() {
	if ( URLArray [ 2 ] )
		return URLArray[ 2 ];
	else if ( URLArray[ 1 ] )
		return URLArray[ 1 ];
	else
		return URLArray[ 0 ];

	return NULL;
}

static char *getFirstURL() {
	return URLArray[ 0 ];
}

static char *getSecondURL() {
	return URLArray[ 1 ];
}

static char *getThirdURL() {
	return URLArray[ 2 ];
}

// static char *getPreviousURL() {
// 	if ( URLArray [ 2 ] )
// 		return URLArray[ 1 ];
// 	else if ( URLArray[ 1 ] )
// 		return URLArray[ 0 ];
// 	else
// 		return NULL;
//
// 	return NULL;
// }

static u_int64_t getFirstAndLastTimeDifference( void ) {
	if ( !URLArray[ 0 ] && !URLArray[ 2 ] )
		return -EXIT_FAILURE;

	return timeArray[ 2 ] - timeArray[ 0 ];
}
static pthread_mutex_t refreshMutex = PTHREAD_MUTEX_INITIALIZER;

static int isRefreshRequest( const char *URL ) {
	u_int64_t currentTime;
	u_int64_t timeDifference;
	static u_int64_t disableTimeStamp = 0;
	static u_int64_t previousSyscallTime = 0;
	int skip = 0;
	int refresh = 0;
	int depth;

	struct syscallContext *context;

	if ( strncmp( URL, "network://Shortcut", 18 ) == 0 )
		return 0;

	depth = getDepth( URL );
	if ( depth > 3 )
		return 0;

	context = getSyscallContext();
	if ( !context->application ) {
		if ( !isDaemonProcess( context ) ) {
			LNCdebug( LNC_DEBUG, 0, "isRefreshRequest( %s ) -> isDaemonProcess( %s ): This application is not a daemon process, enabling", URL, context->applicationName );
			addApplicationEntry( context->applicationName, NULL );
			context->application = findListEntry( applicationList, context->applicationName );
		} else {
			LNCdebug( LNC_DEBUG, 0, "isRefreshRequest( %s ) -> isDaemonProcess( %s ): This application is a daemon process, disabling", URL, context->applicationName );
			addApplicationEntry( context->applicationName, "Null,Yes,Yes,Yes,Yes" );
			context->application = findListEntry( applicationList, context->applicationName );
		}

		if ( !context->application || context->application->disabled )
			return 0;
	}

	if ( context->application->disabled )
		return 0;

	pthread_mutex_lock( &refreshMutex );
	currentTime = getCurrentTime();
	if ( ( ( currentTime - previousSyscallTime ) > 130000ULL ) &&
	        ( ( currentTime - disableTimeStamp ) > 5000000ULL ) ) {

		if ( ( currentTime - previousSyscallTime ) < 500000ULL ) {
			if ( getLastURL() && ( strcmp( getLastURL(), URL ) != 0 ) ) {
				LNCdebug( LNC_DEBUG, 0, "isRefreshRequest( %s ): skipping URL", URL );
				skip = 1;
			}
		}

		if ( skip == 0 ) {
			LNCdebug( LNC_DEBUG, 0, "isRefreshRequest( %s ): adding to URL cache", URL );
			addURL( URL, currentTime );

			if ( getThirdURL() ) {
				if ( strcmp( getFirstURL(), getThirdURL() ) != 0 || strcmp( getSecondURL(), getThirdURL() ) != 0 ) {
					LNCdebug( LNC_DEBUG, 0, "isRefreshRequest( %s ): different path, clearing URL cache", URL );
					clearURLCache( 0 );
				} else {
					timeDifference = getFirstAndLastTimeDifference();
					if ( timeDifference > 0 && timeDifference <= 2000000ULL ) {
						LNCdebug( LNC_DEBUG, 0, "isRefreshRequest( %s ): Forcing update", URL );
						clearURLCache( 1 );
						disableTimeStamp = currentTime;
						refresh = 1;
					}
				}
			}
		}
		previousSyscallTime = currentTime;
	}
	previousSyscallTime += 5000ULL;
	pthread_mutex_unlock( &refreshMutex );

	return refresh;
}

static int filterRequest( const char *path ) {
	int depth;
	char *ptr;

	depth = getPathDepth( path );
	if ( depth < 3 ) {
		ptr = strrchr( path, '/' );
		if ( ptr ) {
			if ( strcasecmp( ptr+1, ".directory" ) == 0 )
				return 0;
			else if ( strncasecmp( ptr, "index.htm", 9 ) == 0 )
				return 1;
			else if ( *( ptr+1 ) == '.' )
				return 1;
		}
	}

	return 0;
}

static int fake_opendir( const char *path, struct fuse_file_info *info ) {
	NETWORK_DIR *dir;

	dir = LNCopendir( "network://", 0 );
	if ( !dir ) {
		LNCdebug( LNC_ERROR, 1, "fake_opendir( %s ) -> LNCopendir( SMB:// )", path );
		info->fh = ( unsigned long ) NULL;
		return -errno;
	}

	info->fh = ( unsigned long ) dir;
	return 0;
}

static int fake_getattr( const char *path, struct stat *stbuf ) {
	( void ) path;
	return -EACCES;//LNCstat( "network://", stbuf );
}

static int networkclientfs_getattr( const char *path, struct stat *stbuf ) {
	int res = 0;
	char buf[ PATH_MAX ];
	int depth;
	struct syscallContext *context;

	if ( filterRequest( path ) ) {
		errno = ENOENT;
		return -ENOENT;
	}

	context = setSyscallContext( NETWORKCLIENT_STAT );
	res = LNCstat( buildURL( buf, path, NULL ) , stbuf );
//	printf( "LNCstat( %s, %s)\n", buf, path );
	if ( res < 0 ) {
		if ( errno == EACCES && context->application && context->application->fakeSyscall ) {
			printf( "networkclientfs_getattr( %s ) = %s, returning fake stat\n", path, strerror_r( errno, buf, PATH_MAX ) );
			res = fake_getattr( path, stbuf );
		}

		if ( errno == ENODEV )
			errno = ENOENT;
		if ( res < 0 )
			printf( "networkclientfs_getattr( %s ) = %s\n", path, strerror_r( errno, buf, PATH_MAX ) );
	} else {
		depth = getPathDepth( path );
		if ( ( depth == 1 && S_ISLNK( stbuf->st_mode ) ) || ( depth >= 2 && depth <= 4 ) )
			stbuf->st_mtime = stbuf->st_ctime = LNCStart.tv_sec + 120;
		else if ( depth == 1 )
			stbuf->st_mtime = stbuf->st_ctime = LNCStart.tv_sec;
	}

	updateApplicationContext( context );

	return ( res == 0 ) ? 0 : -errno;
}

static int networkclientfs_readlink( const char *path, char *buf, size_t bufsize ) {
	int res;
	char *ptr;
	char tmp[ PATH_MAX ];

	res = LNCreadlink( buildURL( tmp, path, NULL ), buf, bufsize );
	if ( res < 0 ) {
		if ( errno == ENODEV )
			errno = ENOENT;
		printf( "networkclientfs_readlink( %s ) = %s\n", path, strerror_r( errno, tmp, PATH_MAX ) );
		return -errno;
	}

	snprintf( tmp, PATH_MAX, "%s", buf );
	ptr = strstr( tmp , "network://" );
	if ( ptr )
		snprintf( buf, bufsize, "%s/%s", mountPath, ptr+10 );

	return 0;
}

static int networkclientfs_opendir( const char *path, struct fuse_file_info *info ) {
	NETWORK_DIR * dir;
	char buf[ PATH_MAX ];
	char *URL;
	unsigned int flags = 0;
	int retval;
	struct syscallContext *context;

	context = setSyscallContext( NETWORKCLIENT_OPENDIR );
	URL = buildURL( buf, path, &flags );
	if ( isRefreshRequest( URL ) )
		flags |= FORCE_RESCAN;

//	setConnectionParameters( flags );
	dir = LNCopendir( URL, flags );
//	clearConnectionParameters();
	if ( !dir ) {
		if ( errno == EACCES && context->application && context->application->fakeSyscall ) {
			printf( "networkclientfs_opendir( %s ) = %s, returning fake opendir\n", path, strerror_r( errno, buf, PATH_MAX ) );
			retval = fake_opendir( path, info );
			if ( retval == 0 )
				dir = ( NETWORK_DIR * ) info->fh;
		}

		if ( errno == ENODEV )
			errno = ENOENT;

		if ( !dir )
			printf( "networkclientfs_opendir( %s ) = %s\n", path, strerror_r( errno, buf, PATH_MAX ) );
	}

	info->fh = ( unsigned long ) dir;
	updateApplicationContext( context );

	return ( dir ) ? 0 : -errno;
}

static int networkclientfs_readdir( const char *path, void *buffer, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *info ) {
	NETWORK_DIR * dir;
	NETWORK_DIRENT dirent;
	NETWORK_DIRENT *result;
	int retval;
	int fillerRetval;

	( void ) offset;
	( void ) path;

	retval = 0;
	dir = ( NETWORK_DIR * ) info->fh;
	if ( strcmp( dir->URL, "network://" ) == 0 ) {
		dir = LNCopendir( "network://Shortcut", 0 );
		if ( dir ) {
			while ( ( ( retval = LNCreaddir( dir, &dirent, &result ) ) == 0 ) && result ) {
				fillerRetval = filler( buffer, result->name, NULL, 0 );
				if ( fillerRetval != 0 ) {
					LNCdebug( LNC_ERROR, 1, "networkclientfs_readdir( %s ) -> filler( %s )", path, result->name );
					break;
				}
			}

			if ( retval != 0 )
				LNCdebug( LNC_ERROR, 1, "networkclientfs_readdir( %s ) -> LNCreaddir( %s )", path, dir->URL );

			LNCclosedir( dir );
		}
	}

	dir = ( NETWORK_DIR * ) info->fh;
	while ( ( ( retval = LNCreaddir( dir, &dirent, &result ) ) == 0 ) && result ) {
		if ( strcmp( result->name, "Shortcut" ) == 0 )
			continue;

		if ( strcmp( dir->URL, "network://" ) == 0 && result->type == DT_DIR )
			fillerRetval = filler( buffer, result->comment, NULL, 0 );
		else
			fillerRetval = filler( buffer, result->name, NULL, 0 );

		if ( fillerRetval != 0 ) {
			LNCdebug( LNC_ERROR, 1, "networkclientfs_readdir( %s ) -> filler( %s )", path, result->name );
			break;
		}
	}

	if ( retval != 0 )
		LNCdebug( LNC_ERROR, 1, "networkclientfs_readdir( %s ) -> LNCreaddir( %s )", path, dir->URL );

	return retval;
}

static int networkclientfs_releasedir( const char *path, struct fuse_file_info *info ) {
	NETWORK_DIR *dir;

	( void ) path;

	dir = ( NETWORK_DIR * ) info->fh;

	return LNCclosedir( dir );
}

static int networkclientfs_mknod( const char *path, mode_t mode, dev_t rdev ) {
	int res = -1;
	char buf[ PATH_MAX ];

	( void ) rdev;

	res = LNCmknod( buildURL( buf, path, NULL ), mode );
	if ( res < 0 ) {
		if ( errno == ENODEV )
			errno = ENOENT;
		printf( "networkclientfs_mknod( %s ) = %s\n", path, strerror_r( errno, buf, PATH_MAX ) );
		return -errno;
	}

	return 0;
}

static int networkclientfs_mkdir( const char *path, mode_t mode ) {
	int res = -1;
	char buf[ PATH_MAX ];

	res = LNCmkdir( buildURL( buf, path, NULL ), mode );
	if ( res < 0 ) {
		if ( errno == ENODEV )
			errno = ENOENT;
		printf( "networkclientfs_mkdir( %s ) = %s\n", path, strerror_r( errno, buf, PATH_MAX ) );
		return -errno;
	}

	return 0;
}

static int networkclientfs_unlink( const char *path ) {
	int res = -1;
	char buf[ PATH_MAX ];

	res = LNCunlink( buildURL( buf, path, NULL ) );
	if ( res < 0 ) {
		if ( errno == ENODEV )
			errno = ENOENT;
		printf( "networkclientfs_unlink( %s ) = %s\n", path, strerror_r( errno, buf, PATH_MAX ) );
		return -errno;
	}

	return 0;
}

static int networkclientfs_rmdir( const char *path ) {
	int res = -1;
	char buf[ PATH_MAX ];

	res = LNCrmdir( buildURL( buf, path, NULL ) );
	if ( res < 0 ) {
		if ( errno == ENODEV )
			errno = ENOENT;
		printf( "networkclientfs_rmdir( %s ) = %s\n", path, strerror_r( errno, buf, PATH_MAX ) );
		return -errno;
	}

	return 0;
}

/*static int networkclientfs_symlink( const char *from, const char *to ) {
	( void ) from;
	( void ) to;
	return -ENOSYS;
}

static int networkclientfs_link( const char *from, const char *to ) {
	( void ) from;
	( void ) to;
	return -ENOSYS;
}*/

static int networkclientfs_rename( const char *from, const char *to ) {
	int res;
	char bufFrom[ PATH_MAX ];
	char bufTo[ PATH_MAX ];

	res = LNCrename( buildURL( bufFrom, from, NULL ), buildURL( bufTo, to, NULL ) );
	if ( res < 0 ) {
		if ( errno == ENODEV )
			errno = ENOENT;
		printf( "networkclientfs_rename( %s, %s ) = %s\n", from, to, strerror_r( errno, bufFrom, PATH_MAX ) );
		return -errno;
	}

	return 0;
}

static int networkclientfs_chmod( const char *path, mode_t mode ) {
	int res = -1;
	char buf[ PATH_MAX ];

	res = LNCchmod( buildURL( buf, path, NULL ), mode );
	if ( res < 0 ) {
		if ( errno == ENODEV )
			errno = ENOENT;
		printf( "networkclientfs_chmod( %s ) = %s\n", path, strerror_r( errno, buf, PATH_MAX ) );
		return -errno;
	}

	return 0;
}

static int networkclientfs_chown( const char *path, uid_t uid, gid_t gid ) {
	int res = -1;
	char buf[ PATH_MAX ];

	res = LNCchown( buildURL( buf, path, NULL ), uid, gid );
	if ( res < 0 ) {
		if ( errno == ENODEV )
			errno = ENOENT;
		printf( "networkclientfs_chown( %s ) = %s\n", path, strerror_r( errno, buf, PATH_MAX ) );
		return -errno;
	}

	return 0;
}

static int networkclientfs_truncate( const char *path, off_t size ) {
	int res = -1;
	char buf[ PATH_MAX ];

	res = LNCtruncate( buildURL( buf, path, NULL ), size );
	if ( res < 0 ) {
		if ( errno == ENODEV )
			errno = ENOENT;
		printf( "networkclientfs_truncate( %s ) = %s\n", path, strerror_r( errno, buf, PATH_MAX ) );
		return -errno;
	}

	return 0;
}

static int networkclientfs_utime( const char *path, struct utimbuf *buf ) {
	int res = -1;
	char tmp[ PATH_MAX ];

	res = LNCutime( buildURL( tmp, path, NULL ), buf );
	if ( res < 0 ) {
		if ( errno == ENODEV )
			errno = ENOENT;
		printf( "networkclientfs_utime( %s ) = %s\n", path, strerror_r( errno, tmp, PATH_MAX ) );
		return -errno;
	}

	return 0;
}

static int networkclientfs_open( const char *path, struct fuse_file_info *info ) {
	NETWORK_FILE * file;
	char buf[ PATH_MAX ];

	file = LNCopen( buildURL( buf, path, NULL ), info->flags, 0 );
	if ( !file ) {
		if ( errno == ENODEV )
			errno = ENOENT;
		printf( "networkclientfs_open( %s, %d ) = %s\n", path, info->flags, strerror_r( errno, buf, PATH_MAX ) );
		return -errno;
	}

	info->fh = ( unsigned long ) file;

	return 0;
}

static int networkclientfs_read( const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *info ) {
	NETWORK_FILE * file;
	char tmp[ PATH_MAX ];
	int res;

	file = ( NETWORK_FILE * ) info->fh;

	if ( offset != 0 ) {
		res = LNClseek( file, offset, SEEK_SET );
		if ( res != offset )
			goto error;
	}

	res = LNCread( file, buf, size );
	if ( res < 0 )
		goto error;

	return res;

error:
	if ( errno == ENODEV )
		errno = ENOENT;
	printf( "networkclientfs_read( %s ) = %s\n", path, strerror_r( errno, tmp, PATH_MAX ) );
	return -errno;
}

static int networkclientfs_write( const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *info ) {
	NETWORK_FILE * file;
	char tmp[ PATH_MAX ];
	int res;

	file = ( NETWORK_FILE * ) info->fh;

	if ( offset != 0 ) {
		res = LNClseek( file, offset, SEEK_SET );
		if ( res != offset )
			goto error;
	}

	res = LNCwrite( file, ( void * ) buf, size );
	if ( res < 0 )
		goto error;

	return res;

error:
	if ( errno == ENODEV )
		errno = ENOENT;
	printf( "networkclientfs_write( %s ) = %s\n", path, strerror_r( errno, tmp, PATH_MAX ) );
	return -errno;
}

static int networkclientfs_statfs( const char *path, struct statfs *buf ) {
	int retval;
	char tmp[ PATH_MAX ];

	retval = statfs( path, buf );
	if ( retval < 0 ) {
		if ( errno == ENODEV )
			errno = ENOENT;
		printf( "networkclientfs_statfs( %s ) = %s\n", path, strerror_r( errno, tmp, PATH_MAX ) );
		return -errno;
	}

	return 0;
}

static int networkclientfs_release( const char *path, struct fuse_file_info *info ) {
	NETWORK_FILE *file;

	( void ) path;

	file = ( NETWORK_FILE * ) info->fh;

	LNCclose( file );

	return 0;
}

static int networkclientfs_fsync( const char *path, int isdatasync, struct fuse_file_info *info ) {
	( void ) info;
	( void ) path;
	( void ) isdatasync;
	return 0;
}

static struct fuse_operations networkclientfsOperations = {
	        .getattr = networkclientfs_getattr,
	                   .readlink = networkclientfs_readlink,
	                               .opendir = networkclientfs_opendir,
	                                          .readdir = networkclientfs_readdir,
	                                                     .releasedir = networkclientfs_releasedir,
	                                                                   /*.getdir = networkclientfs_getdir,*/
	                                                                   .mknod = networkclientfs_mknod,
	                                                                            .mkdir = networkclientfs_mkdir,
	                                                                                     /*.symlink = networkclientfs_symlink,*/
	                                                                                     .unlink = networkclientfs_unlink,
	                                                                                               .rmdir = networkclientfs_rmdir,
	                                                                                                        .rename = networkclientfs_rename,
	                                                                                                                  /*.link = networkclientfs_link,*/
	                                                                                                                  .chmod = networkclientfs_chmod,
	                                                                                                                           .chown = networkclientfs_chown,
	                                                                                                                                    .truncate = networkclientfs_truncate,
	                                                                                                                                                .utime = networkclientfs_utime,
	                                                                                                                                                         .open = networkclientfs_open,
	                                                                                                                                                                 .read = networkclientfs_read,
	                                                                                                                                                                         .write = networkclientfs_write,
	                                                                                                                                                                                  .statfs = networkclientfs_statfs,
	                                                                                                                                                                                            .release = networkclientfs_release,
	                                                                                                                                                                                                       .fsync = networkclientfs_fsync,
                                                                                                                                                                                                                };

void *makeDirs( void *arg ) {
	int i;
	int retval;
	char path[ PATH_MAX ];
	NETWORK_DIR *directory;

	( void ) arg;

	sleep( 2 );

	directory = LNCopendir( "network://SMB/LINUX/USARIN/F", 0 );
	if ( !directory ) {
		perror( "LNCopendir" );
		return NULL;
	}

	for ( i = 0; i < 1000; ++i ) {
		snprintf( path, PATH_MAX, "/home/usarin/tmp/network/%d/SMB/LINUX/USARIN/F/home/usarin/test/%d", getpid(), i );
//		snprintf( path, PATH_MAX, "/home/usarin/test/%d", i );
		retval = mkdir( path, 0755 );
		if ( retval < 0 )
			printf( "mkdir %s = %s\n", path, strerror( errno ) );
	}

	sleep( 10 );

	for ( i = 0; i < 1000; ++i ) {
		snprintf( path, PATH_MAX, "/home/usarin/tmp/network/%d/SMB/LINUX/USARIN/F/home/usarin/test/%d", getpid(), i );
//		snprintf( path, PATH_MAX, "/home/usarin/test/%d", i );
		retval = rmdir( path );
		if ( retval < 0 )
			printf( "rmdir %s = %s\n", path, strerror( errno ) );
	}

	LNCclosedir( directory );

	return NULL;
}

int fuseMount( const char *mountPoint ) {
	int fd;
	int retval;
	char args[] = "fsname=netfs,allow_other"; //"direct_io";
	char buffer[ BUFSIZ ];
	struct passwd *user;

	user = getSessionInfoByUID( geteuid(), buffer );
	if ( !user ) {
		LNCdebug( LNC_ERROR, 1, "fuseMount( %s ) -> getSessionInfoByUID( %d )", mountPoint, geteuid() );
		return -EXIT_FAILURE;
	}
	sprintf( homeDirectory, "%s", user->pw_dir );

	fuse_unmount( mountPoint );

	retval = buildPath( mountPoint, 0755 );
	if ( retval < 0 ) {
		LNCdebug( LNC_ERROR, 1, "fuseMount() -> buildPath( %s )", mountPoint );
		return -EXIT_FAILURE;
	}

	fd = fuse_mount( mountPoint, ( const char * ) args );
	if ( fd < 0 ) {
		errno = fd * -1;
		LNCdebug( LNC_ERROR, 1, "fuseMount( \"%s\" ) -> fuse_mount()", mountPoint );
		return -EXIT_FAILURE;
	}

	fuse = fuse_new( fd, "hard_remove,allow_root", &networkclientfsOperations, sizeof( networkclientfsOperations ) );
	if ( !fuse ) {
		LNCdebug( LNC_ERROR, 1, "fuseMount( \"%s\" ) -> fuse_new()", mountPoint );
		return -EXIT_FAILURE;
	}

//	createThread( makeDirs, NULL, DETACHED );

	lockLNCConfig( READ_LOCK );
	if ( networkConfig.EnableDebugMode ) {
		unlockLNCConfig();
		fuse_loop( fuse );
	} else {
		unlockLNCConfig();
		fuse_loop_mt( fuse );
	}

	fuse_destroy( fuse );
	close( fd );
	fuse_unmount( mountPoint );

	retval = rmdir( mountPoint );
	if ( retval < 0 ) {
		LNCdebug( LNC_ERROR, 1, "fuseMount( \"%s\" ) -> rmdir( %s )", mountPoint, mountPoint );
		return -EXIT_FAILURE;
	}

	clearURLCache( 1 );

	return EXIT_SUCCESS;
}
