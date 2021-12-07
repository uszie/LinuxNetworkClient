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
#include "config.h"
#include "nconfig.h"
#include "util.h"
#include "plugin.h"
#include "syscall.h"

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <kdb.h>

#include <limits.h>
#include <sys/types.h>
#include <fcntl.h>
#include <dirent.h>

struct timeval LNCStart;
char *enabledPlugins = NULL;
int networkPluginsOnly = 1;
int ( *LNCauthorizationCallback ) ( const char *URL, struct auth *auth );
// char *LNCStringError( int error );

//static pthread_key_t connectionKey = 0;
//static pthread_key_t authenticationKey = 0;
//static pthread_key_t readdirKey = 0;

// void setConnectionParameters( unsigned int flags ) {
// 	void * buffer;
// 	unsigned int *tmp;
//
// 	if ( flags == 0 )
// 		return ;
//
// 	buffer = createTSDBuffer( sizeof( unsigned int ), &connectionKey, 1 );
// 	if ( !buffer ) {
// 		LNCdebug( LNC_ERROR, 1, "setConnectionParameters() -> createTSDBuffer()" );
// 		return ;
// 	}
//
// 	tmp = ( unsigned int * ) buffer;
// 	*tmp = flags;
// }
//
// void clearConnectionParameters( void ) {
// 	void * buffer;
// 	unsigned int *tmp;
//
// 	buffer = createTSDBuffer( sizeof( unsigned int ), &connectionKey, 0 );
// 	if ( buffer ) {
// 		tmp = ( unsigned int * ) buffer;
// 		*tmp = 0;
// 	}
// }
//
// unsigned int getConnectionParameters( void ) {
// 	void * buffer;
// 	unsigned int *tmp;
//
// 	buffer = createTSDBuffer( sizeof( unsigned int ), &connectionKey, 0 );
// 	if ( buffer ) {
// 		tmp = ( unsigned int * ) buffer;
// 		return *tmp;
// 	}
//
// 	return 0;
// }



int getDepth( const char *URL ) {
	char cpy[ PATH_MAX ];
	char *cpyPtr = cpy;
	char *ptr;
	char *token;
	int i;

	strlcpy( cpy, URL, PATH_MAX );
	for ( i = 0; ( token = strtok_r( cpyPtr, "/", &ptr ) ); ++i ) {
		cpyPtr = NULL;
		if ( i == 7 )
			break;
	}

	return i;
}


int LNCinit( int loadNetworkPluginsOnly, const char *pluginsToLoad ) {
	int retval;
	char registryPath[ PATH_MAX ];
	char buffer[ BUFSIZ ];
	struct passwd *user;

	gettimeofday( &LNCStart, NULL );
	keysDBHandle = ( void * ) kdbOpen();
	if ( !keysDBHandle ) {
		LNCdebug( LNC_ERROR, 1, "LNCinit( %d, %s ) -> kdbOpen()", loadNetworkPluginsOnly, pluginsToLoad );
		return -EXIT_FAILURE;
	}

	lockLNCConfig( WRITE_LOCK );
	freeNetworkConfiguration();
	readNetworkConfiguration();
	printNetworkConfiguration();
	unlockLNCConfig();

	user = getSessionInfoByUID( geteuid(), buffer );
	if ( !user ) {
		LNCdebug( LNC_ERROR, 1, "LNCinit( %d, %s ) -> getSessionInfoByUID( %d )", loadNetworkPluginsOnly, pluginsToLoad, geteuid() );
		kdbClose( keysDBHandle );
		freeNetworkConfiguration();
		return -EXIT_FAILURE;
	}

	if ( pluginsToLoad )
		enabledPlugins = strdup( pluginsToLoad );

 	networkPluginsOnly = loadNetworkPluginsOnly;
 	retval = loadPlugins();
 	if ( retval < 0 ) {
 		LNCdebug( LNC_ERROR, 1, "LNCinit( %d, %s ) -> loadPlugins()", loadNetworkPluginsOnly, pluginsToLoad );//
 		return -EXIT_FAILURE;
 	}

	sprintf( registryPath, "%s/.kdb/user/sw/LNC", user->pw_dir );
	addFileToMonitorList( registryPath, updateNetworkConfiguration );
	addFileToMonitorList( LNC_PLUGIN_DIR, updatePluginConfiguration );

//	sprintf( registryPath, "%s/.kdb/user/sw/LNC/hosts", user->pw_dir );
//	addFileToMonitorList( registryPath, updateManualHosts );
//	updateManualHosts( "void", "void", FAMChanged );

	return EXIT_SUCCESS;
}

void LNCcleanUp( void ) {
	LNCdebug( LNC_INFO, 0, "LNCcleanUp(): cleaning up" );
	clearSyscallHooks();
	stopFileMonitor();
	unloadPlugins();
	lockLNCConfig ( WRITE_LOCK );
	freeNetworkConfiguration();
	unlockLNCConfig ();
	freeAllTSDBuffers();

	if ( enabledPlugins )
		free( enabledPlugins );

	if ( networkclient_stdout )
		fclose( networkclient_stdout );

	kdbClose( ( KDB * ) keysDBHandle );
}

/* char *LNCStringError( int error ) {
	static char errorString[] = "LNCStringError(): Internal error";
	char *buffer;
	static pthread_key_t key = 0;

	buffer = createTSDBuffer( BUFSIZ, &key );
	if ( !buffer )
		return errorString;

	if ( strerror_r( error, buffer, BUFSIZ ) < 0 )
		return errorString;

	return buffer;
}*/

char *getProtocolType( const char *URL, char *buffer, size_t size ) {
	char * begin;
	char *end;
	size_t len;

	begin = strstr( URL, "://" );
	if ( !begin || begin[ 3 ] == '\0' )
		return NULL;

	begin += 3;
	end = strchr( begin, '/' );
	if ( end ) {
		len = ( end - begin ) + 1 ;
		if ( len > size )
			len = size;
	} else
		len = size;

	strlcpy( buffer, begin, len );

	return buffer;
}

int LNCaddManualHost( const char *URL, const char *name ) {
	int retval;
	NETWORK_DIR *dir;
	Service *found;


	dir = LNCopendir( URL, 0 );
	if ( !dir ) {
		LNCdebug( LNC_ERROR, 1, "LNCaddManualHost( %s ) -> LNCopendir( %s )", URL, URL );
		return -EXIT_FAILURE;
	}

	LNCclosedir( dir );

	found = findListEntry( browselist, URL );
	if ( !found ) {
		LNCdebug( LNC_ERROR, 1, "LNCaddManualHost( %s ) -> findListEntry( browselist, %s )", URL, URL );
		return -EXIT_FAILURE;
	}

	if ( !found->host ) {
		LNCdebug( LNC_ERROR, 0, "LNCaddManualHost( %s ): Is not a valid host", URL );
		putListEntry( browselist, found );
		return -EXIT_FAILURE;
	}

	setBusy( found );
	found->hostType = MANUAL_HOST;
	unsetBusy( found );
	putListEntry( browselist, found );

	initKeys( "LNC/hosts" );
	retval = setCharKey( "LNC/hosts", name, URL, USER_ROOT );
	if ( retval < 0 ) {
		LNCdebug( LNC_ERROR, 1, "LNCaddManualHost( %s ) -> setNumKey( \"LNC/hosts\", %s, %s, USER_ROOT )", URL, name, URL );
		return -EXIT_FAILURE;
	}

	cleanupKeys( "LNC/hosts" );

	return EXIT_SUCCESS;
}

int LNCdelManualHost( const char *URL ){

}

NETWORK_FILE *LNCopen( const char *URL, unsigned int flags, mode_t mode ) {
	char * protocol;
	char errorString[ BUFSIZ ];
	char buffer[ NAME_MAX + 1 ];
	int replace;
	int oldCancelState;
	NetworkPlugin *plugin;
	NETWORK_FILE *file;
	NETWORK_FILE *tmp = NULL;

	pthread_setcancelstate( PTHREAD_CANCEL_DISABLE, &oldCancelState );
	file = runLNCopenHooks( URL, flags, mode, &replace, PRE_SYSCALL, NULL );
	if ( replace )
		goto out;

	if ( strcmp( URL, "network://" ) == 0 ) {
		errno = EISDIR;
		goto out;
	}

	protocol = getProtocolType( URL, buffer, NAME_MAX + 1 );
	if ( !protocol ) {
		LNCdebug( LNC_ERROR, 1, "LNCopen( %s, %d, %d ) -> getProtocolType()", URL, flags, mode );
		goto out;
	}

	plugin = findNetworkPlugin( protocol );
	if ( !plugin ) {
		LNCdebug( LNC_ERROR, 1, "LNCopen( %s, %d, %d ) -> findNetworkPlugin( %s )", URL, flags, mode, protocol );
		goto out;
	}

	if ( !plugin->open && !plugin->mount ) {
		putNetworkPlugin( plugin );
		errno = ENOSYS;
		goto out;
	}

	tmp = malloc( sizeof( NETWORK_FILE ) );
	if ( !tmp ) {
		LNCdebug( LNC_ERROR, 1, "LNCopen( %s, %d ) -> malloc()", URL, flags );
		putNetworkPlugin( plugin );
		goto out;
	}

	strlcpy( tmp->URL, URL, PATH_MAX );
	tmp->privateData = NULL;
	tmp->plugin = plugin;

	if ( plugin->open && ( plugin->type != USE_BROWSELIST ) )
		file = plugin->open( URL, flags, mode, tmp );
	else
		file = DNPopen( URL, flags, mode, tmp, plugin );
	if ( !file ) {
		free( tmp );
		putNetworkPlugin( plugin );
		goto out;
	}

out:
	file = runLNCopenHooks( URL, flags, mode, &replace, AFTER_SYSCALL, file );

	LNCdebug( LNC_DEBUG, 0, "LNCopen( %s, %d ) = %s", URL, flags, strerror_r( file ? 0 : errno, errorString, BUFSIZ ) );
	pthread_setcancelstate( oldCancelState, NULL );

	return file;
}

int LNCclose( NETWORK_FILE *file ) {
	NetworkPlugin * plugin;
	int retval;
	int replace;
	int oldCancelState;

	pthread_setcancelstate( PTHREAD_CANCEL_DISABLE, &oldCancelState );
	retval = runLNCcloseHooks( file, &replace, PRE_SYSCALL, -EXIT_FAILURE );
	if ( replace )
		goto out;

	plugin = file->plugin;
	if ( !plugin->close && !plugin->mount ) {
		free( file );
		putNetworkPlugin( plugin );
		errno = ENOSYS;
		goto out;
	}

	if ( plugin->close && ( plugin->type != USE_BROWSELIST ) )
		retval = plugin->close( file );
	else
		retval = DNPclose( file, plugin );

	free( file );
	putNetworkPlugin( plugin );

out:
	retval = runLNCcloseHooks( file, &replace, AFTER_SYSCALL, retval );
	pthread_setcancelstate( oldCancelState, NULL );

	return retval;
}

NETWORK_DIR *LNCopendir( const char *URL, unsigned int flags ) {
	char * protocol;
	char errorString[ BUFSIZ ];
	char buffer[ NAME_MAX + 1 ];
	int replace;
	int oldCancelState;
	NetworkPlugin *plugin;
	NETWORK_DIR *dir;
	NETWORK_DIR *tmp;

	pthread_setcancelstate( PTHREAD_CANCEL_DISABLE, &oldCancelState );
	dir = runLNCopendirHooks( URL, flags, &replace, PRE_SYSCALL, NULL );
	if ( replace )
		goto out;

	tmp = malloc( sizeof( NETWORK_DIR ) );
	if ( !tmp ) {
		LNCdebug( LNC_ERROR, 1, "LNCopendir( %s, %u ) -> malloc()", URL, flags );
		goto out;
	}

	if ( strcmp( URL, "network://" ) == 0 ) {
		strlcpy( tmp->URL, URL, PATH_MAX );
		tmp->privateData = ( void * ) 0;
		tmp->plugin = NULL;
//		tmp->depth = 1;
		dir = tmp;
		goto out;
	}

	protocol = getProtocolType( URL, buffer, NAME_MAX + 1 );
	if ( !protocol ) {
		LNCdebug( LNC_ERROR, 1, "LNCopendir( %s, %u ) -> getProtocolType()", URL, flags );
		free( tmp );
		goto out;
	}

	plugin = findNetworkPlugin( protocol );
	if ( !plugin ) {
		LNCdebug( LNC_ERROR, 1, "LNCopendir( %s, %u ) -> findNetworkPlugin( %s )", URL, flags, protocol );
		free( tmp );
		goto out;
	}

	if ( !plugin->opendir && !plugin->mount ) {
		free( tmp );
		putNetworkPlugin( plugin );
		errno = ENOSYS;
		goto out;
	}

	strlcpy( tmp->URL, URL, PATH_MAX );
	tmp->privateData = NULL;
	tmp->plugin = plugin;

	if ( plugin->opendir && ( plugin->type != USE_BROWSELIST ) )
		dir = plugin->opendir( URL, flags, tmp );
	else
		dir = DNPopendir( URL, flags, tmp, plugin );
	if ( !dir ) {
		free( tmp );
		putNetworkPlugin( plugin );
		goto out;
	}

out:
	dir = runLNCopendirHooks( URL, flags, &replace, AFTER_SYSCALL, dir );

	LNCdebug( LNC_DEBUG, 0, "LNCopendir( %s ) = %s", URL, strerror_r( dir ? 0 : errno, errorString, BUFSIZ ) );
	pthread_setcancelstate( oldCancelState, NULL );

	return dir;
}

int LNCclosedir( NETWORK_DIR *dir ) {
	NetworkPlugin * plugin;
	int retval;
	int replace;
	int oldCancelState;

	pthread_setcancelstate( PTHREAD_CANCEL_DISABLE, &oldCancelState );
	retval = runLNCclosedirHooks( dir, &replace, PRE_SYSCALL, -EXIT_FAILURE );
	if ( replace )
		goto out;

	if ( !dir->plugin && strcmp( dir->URL, "network://" ) == 0 ) {
		free( dir );
		goto out;
	}

	plugin = dir->plugin;
	if ( !plugin->closedir && !plugin->mount ) {
		free( dir );
		putNetworkPlugin( plugin );
		errno = ENOSYS;
		retval = -EXIT_FAILURE;
		goto out;
	}

	if ( plugin->closedir && ( plugin->type != USE_BROWSELIST ) )
		retval = plugin->closedir( dir );
	else
		retval = DNPclosedir( dir, plugin );

	free( dir );
	putNetworkPlugin( plugin );

out:
	retval = runLNCclosedirHooks( dir, &replace, AFTER_SYSCALL, retval );
	pthread_setcancelstate( oldCancelState, NULL );

	return retval;
}

int LNCreaddir( NETWORK_DIR *dir, NETWORK_DIRENT *entry, NETWORK_DIRENT **result ) {
	char errorString[ BUFSIZ ];
	int i;
	int replace;
	int oldCancelState;
	int retval;
	struct key *key;
	NetworkPlugin *plugin;

	*result = NULL;
	pthread_setcancelstate( PTHREAD_CANCEL_DISABLE, &oldCancelState );
	retval = runLNCreaddirHooks( dir, entry, result, &replace, PRE_SYSCALL, -EXIT_FAILURE );
	if ( replace )
		goto out;

	if ( !dir->plugin && strcmp( dir->URL, "network://" ) == 0 ) {
		lockProtocolList();
		i = 0;
		forEachProtocol( plugin ) {
			if ( i == void_to_int_cast dir->privateData ) {
				dir->privateData += 1;
				entry->type = DT_DIR;
				entry->IP[ 0 ] = '\0';
				strlcpy( entry->name, plugin->protocol, NAME_MAX + 1 );
				strlcpy( entry->comment, plugin->networkName, NAME_MAX + 1 );
				unlockProtocolList();
				*result = entry;
				retval = EXIT_SUCCESS;
				goto out;
			}

			i += 1;
		}
		unlockProtocolList();

		initKeys( "LNC/hosts" );
		rewindKeys( "LNC/hosts", USER_ROOT );
		while ( ( key = forEveryKey2( "LNC/hosts", USER_ROOT ) ) ) {
			if ( key->type != LNC_CONFIG_KEY ) {
				free( key );
				continue;
			}

			plugin = findNetworkPlugin( key->value+10 );
			if ( !plugin ) {
				free( key );
				continue;
			}

			putNetworkPlugin( plugin );

			if ( i == void_to_int_cast dir->privateData ) {
				dir->privateData += 1;
				entry->type = DT_LNK;
				entry->IP[ 0 ] = '\0';
				strlcpy( entry->name, key->name, NAME_MAX + 1 );
				strlcpy( entry->comment, key->name, NAME_MAX + 1 );
				free( key );
				cleanupKeys( "LNC/hosts" );
				*result = entry;
				retval = EXIT_SUCCESS;
				goto out;
			}

			free( key );
			i += 1;
		}
		cleanupKeys( "LNC/hosts" );

		errno = 0;
		retval = EXIT_SUCCESS;
		goto out;
	}

	plugin = dir->plugin;
	if ( !plugin->readdir && !plugin->mount ) {
		errno = ENOSYS;
		retval = -EXIT_FAILURE;
		goto out;
	}

	if ( plugin->readdir && ( plugin->type != USE_BROWSELIST ) )
		retval = plugin->readdir( dir, entry, result );
	else
		retval = DNPreaddir( dir, entry, result, plugin );

out:
	retval = runLNCreaddirHooks( dir, entry, result, &replace, AFTER_SYSCALL, retval );

	LNCdebug( LNC_DEBUG, 0, "LNCreaddir( %s ) = %s", dir->URL, strerror_r( ( ( retval == 0 ) && entry ) ? 0 : errno, errorString, BUFSIZ ) );
	pthread_setcancelstate( oldCancelState, NULL );

	return retval;
}

ssize_t LNCread( NETWORK_FILE *file, void *buf, size_t count ) {
	char errorString[ BUFSIZ ];
	int replace;
	int oldCancelState;
	NetworkPlugin * plugin;
	ssize_t retval;

	pthread_setcancelstate( PTHREAD_CANCEL_DISABLE, &oldCancelState );
	retval = runLNCreadHooks( file, buf, count, &replace, PRE_SYSCALL, ( ssize_t ) -EXIT_FAILURE );
	if ( replace )
		goto out;

	plugin = file->plugin;
	if ( !plugin->read && !plugin->mount ) {
		errno = ENOSYS;
		goto out;
	}

	if ( plugin->read && ( plugin->type != USE_BROWSELIST ) )
		retval = plugin->read( file, buf, count );
	else
		retval = DNPread( file, buf, count, plugin );

out:
	retval = runLNCreadHooks( file, buf, count, &replace, AFTER_SYSCALL, retval );

	LNCdebug( LNC_DEBUG, 0, "LNCread( %s, buf, count ) = %d ( %s )", file->URL, retval, strerror_r( ( retval >= 0 ) ? 0 : errno, errorString, BUFSIZ ) );
	pthread_setcancelstate( oldCancelState, NULL );

	return retval;
}

ssize_t LNCwrite( NETWORK_FILE *file, void *buf, size_t count ) {
	char errorString[ BUFSIZ ];
	int replace;
	int oldCancelState;
	NetworkPlugin * plugin;
	ssize_t retval;

	pthread_setcancelstate( PTHREAD_CANCEL_DISABLE, &oldCancelState );
	retval = runLNCwriteHooks( file, buf, count, &replace, PRE_SYSCALL, ( ssize_t ) -EXIT_FAILURE );
	if ( replace )
		goto out;

	plugin = file->plugin;
	if ( !plugin->write && !plugin->mount ) {
		errno = ENOSYS;
		goto out;
	}

	if ( plugin->write && ( plugin->type != USE_BROWSELIST ) )
		retval = plugin->write( file, buf, count );
	else
		retval = DNPwrite( file, buf, count, plugin );

out:
	retval = runLNCwriteHooks( file, buf, count, &replace, AFTER_SYSCALL, retval );

	LNCdebug( LNC_DEBUG, 0, "LNCwrite( %s, buf, count ) = %d ( %s )", file->URL, retval, strerror_r( ( retval >= 0 ) ? 0 : errno, errorString, BUFSIZ ) );
	pthread_setcancelstate( oldCancelState, NULL );

	return retval;
}

int LNCstat( const char *URL, struct stat *buf ) {
	char *link;
	char *protocol;
	char errorString[ BUFSIZ ];
	char buffer[ NAME_MAX + 1 ];
	int replace;
	int retval;
	int i;
	int savedErrno;
	int oldCancelState;
	NetworkPlugin *plugin;

	pthread_setcancelstate( PTHREAD_CANCEL_DISABLE, &oldCancelState );
	retval = runLNCstatHooks( URL, buf, &replace, PRE_SYSCALL, -EXIT_FAILURE );
	if ( replace )
		goto out;

	if ( strcmp( URL, "network://" ) == 0 ) {
		lockProtocolList();
		i = 0;
		forEachProtocol( plugin )
			i += 1;
		unlockProtocolList();

		buf->st_uid = getuid();
		buf->st_gid = getgid();
		buf->st_nlink = i;
		buf->st_size = i * 24;
		buf->st_mode = S_IFDIR | S_IRWXU;
		buf->st_mtime = buf->st_ctime = buf->st_atime = LNCStart.tv_sec;
		retval = EXIT_SUCCESS;
		goto out;
	}

	protocol = getProtocolType( URL, buffer, NAME_MAX + 1 );
	if ( !protocol ) {
		LNCdebug( LNC_ERROR, 1, "LNCstat( %s ) -> getProtocolType()", URL );
		goto out;
	}

	plugin = findNetworkPlugin( protocol );
	if ( !plugin ) {
		savedErrno = errno;
		initKeys( "LNC/hosts" );
		link = getCharKey( "LNC/hosts", protocol, NULL );
		if ( !link ) {
			cleanupKeys( "LNC/hosts" );
			errno = savedErrno;
			LNCdebug( LNC_ERROR, 1, "LNCstat( %s ) -> findNetworkPlugin( %s )", URL, protocol );
			goto out;
		}
		cleanupKeys( "LNC/hosts" );

		buf->st_uid = getuid();
		buf->st_gid = getgid();
		buf->st_nlink = 0;
		buf->st_size = strlen( link );
		buf->st_mode = S_IFLNK | S_IRWXU;
		buf->st_mtime = buf->st_ctime = buf->st_atime = LNCStart.tv_sec;
		free( link );
		retval = EXIT_SUCCESS;
		goto out;
	}

	if ( !plugin->stat && !plugin->mount ) {
		putNetworkPlugin( plugin );
		errno = ENOSYS;
		goto out;
	}

	if ( plugin->stat && ( plugin->type != USE_BROWSELIST ) )
		retval = plugin->stat( URL, buf );
	else
		retval = DNPstat( URL, buf, plugin );

	putNetworkPlugin( plugin );

out:
	retval = runLNCstatHooks( URL, buf, &replace, AFTER_SYSCALL, retval );

	LNCdebug( LNC_DEBUG, 0, "LNCstat( %s ) = %d ( %s )", URL, retval, strerror_r( ( retval == 0 ) ? 0 : errno, errorString, BUFSIZ ) );
	pthread_setcancelstate( oldCancelState, NULL );

	return retval;
}

off_t LNClseek( NETWORK_FILE *file, off_t offset, int whence ) {
	char errorString[ BUFSIZ ];
	int replace;
	int oldCancelState;
	NetworkPlugin * plugin;
	off_t retval;

	pthread_setcancelstate( PTHREAD_CANCEL_DISABLE, &oldCancelState );
	retval = runLNClseekHooks( file, offset, whence, &replace, PRE_SYSCALL, ( off_t ) -EXIT_FAILURE );
	if ( replace )
		goto out;

	plugin = file->plugin;
	if ( !plugin->lseek && !plugin->mount ) {
		errno = ENOSYS;
		goto out;
	}

	if ( plugin->lseek && ( plugin->type != USE_BROWSELIST ) )
		retval = plugin->lseek( file, offset, whence );
	else
		retval = DNPlseek( file, offset, whence, plugin );

out:
	retval = runLNClseekHooks( file, offset, whence, &replace, AFTER_SYSCALL, retval );

	LNCdebug( LNC_DEBUG, 0, "LNClseek( %s, %lld, %d ) = %lld ( %s )", file->URL, offset, whence, retval, strerror_r( ( retval >= 0 ) ? 0 : errno, errorString, BUFSIZ ) );
	pthread_setcancelstate( oldCancelState, NULL );

	return retval;
}

ssize_t LNCreadlink( const char *URL, char *buf, size_t bufsize ) {
	char *link;
	char * protocol;
	char errorString[ BUFSIZ ];
	char buffer[ NAME_MAX + 1 ];
	int replace;
	int savedErrno;
	int oldCancelState;
	NetworkPlugin *plugin;
	ssize_t retval;
	size_t len;

	pthread_setcancelstate( PTHREAD_CANCEL_DISABLE, &oldCancelState );
	retval = runLNCreadlinkHooks( URL, buf, bufsize, &replace, PRE_SYSCALL, ( ssize_t ) -EXIT_FAILURE );
	if ( replace )
		goto out;

	if ( strcmp( URL, "network://" ) == 0 ) {
		errno = ENOENT;
		goto out;
	}

	protocol = getProtocolType( URL, buffer, NAME_MAX + 1 );
	if ( !protocol ) {
		LNCdebug( LNC_ERROR, 1, "LNCreadlink( %s ) -> getProtocolType()", URL );
		goto out;
	}

	plugin = findNetworkPlugin( protocol );
	if ( !plugin ) {
		savedErrno = errno;
		initKeys( "LNC/hosts" );
		link = getCharKey( "LNC/hosts", protocol, NULL );
		if ( !link ) {
			cleanupKeys( "LNC/hosts" );
			errno = savedErrno;
			LNCdebug( LNC_ERROR, 1, "LNCreadlink( %s ) -> findNetworkPlugin( %s )", URL, protocol );
			goto out;
		}
		cleanupKeys( "LNC/hosts" );

		len = snprintf( buf, bufsize, "%s", link );
		if ( len > bufsize )
			len = bufsize;
		retval = len;
		free( link );
		goto out;

	}

	if ( !plugin->readlink && !plugin->mount ) {
		putNetworkPlugin( plugin );
		errno = ENOSYS;
		goto out;
	}

	if ( plugin->readlink && ( plugin->type != USE_BROWSELIST ) )
		retval = plugin->readlink( URL, buf, bufsize );
	else
		retval = DNPreadlink( URL, buf, bufsize, plugin );

	putNetworkPlugin( plugin );

out:
	retval = runLNCreadlinkHooks( URL, buf, bufsize, &replace, AFTER_SYSCALL, retval );

	LNCdebug( LNC_DEBUG, 0, "LNCreadlink( %s ) = %d ( %s )", URL, retval, strerror_r( ( retval >= 0 ) ? 0 : errno, errorString, BUFSIZ ) );
	pthread_setcancelstate( oldCancelState, NULL );

	return retval;
}

int LNCmknod( const char *URL, mode_t mode ) {
	char * protocol;
	char errorString[ BUFSIZ ];
	char buffer[ NAME_MAX + 1 ];
	int retval;
	int replace;
	int oldCancelState;
	NetworkPlugin *plugin;

	pthread_setcancelstate( PTHREAD_CANCEL_DISABLE, &oldCancelState );
	retval = runLNCmknodHooks( URL, mode, &replace, PRE_SYSCALL, -EXIT_FAILURE );
	if ( replace )
		goto out;

	if ( strcmp( URL, "network://" ) == 0 ) {
		errno = EEXIST;
		goto out;
	}

	protocol = getProtocolType( URL, buffer, NAME_MAX + 1 );
	if ( !protocol ) {
		LNCdebug( LNC_ERROR, 1, "LNCmknod( %s ) -> getProtocolType()", URL );
		goto out;
	}

	plugin = findNetworkPlugin( protocol );
	if ( !plugin ) {
		LNCdebug( LNC_ERROR, 1, "LNCmknod( %s ) -> findNetworkPlugin( %s )", URL, protocol );
		goto out;
	}

	if ( getDepth( URL ) < plugin->minimumTouchDepth ) {
		putNetworkPlugin( plugin );
		errno = EACCES;
		goto out;
	}

	if ( !plugin->mknod && !plugin->mount ) {
		putNetworkPlugin( plugin );
		errno = ENOSYS;
		goto out;
	}

	if ( plugin->mknod && ( plugin->type != USE_BROWSELIST ) )
		retval = plugin->mknod( URL, mode );
	else
		retval = DNPmknod( URL, mode, plugin );

	putNetworkPlugin( plugin );

out:
	retval = runLNCmknodHooks( URL, mode, &replace, AFTER_SYSCALL, retval );

	LNCdebug( LNC_DEBUG, 0, "LNCmknod( %s, %d ) = %d ( %s )", URL, mode, retval, strerror_r( ( retval == 0 ) ? 0 : errno, errorString, BUFSIZ ) );
	pthread_setcancelstate( oldCancelState, NULL );

	return retval;
}

int LNCunlink( const char *URL ) {
	char * protocol;
	char errorString[ BUFSIZ ];
	char buffer[ NAME_MAX + 1 ];
	int retval;
	int replace;
	int oldCancelState;
	NetworkPlugin *plugin;

	pthread_setcancelstate( PTHREAD_CANCEL_DISABLE, &oldCancelState );
	retval = runLNCunlinkHooks( URL, &replace, PRE_SYSCALL, -EXIT_FAILURE );
	if ( replace )
		goto out;

	if ( strcmp( URL, "network://" ) == 0 ) {
		errno = EISDIR;
		goto out;
	}

	protocol = getProtocolType( URL, buffer, NAME_MAX + 1 );
	if ( !protocol ) {
		LNCdebug( LNC_ERROR, 1, "LNCunlink( %s ) -> getProtocolType()", URL );
		goto out;
	}

	plugin = findNetworkPlugin( protocol );
	if ( !plugin ) {
		LNCdebug( LNC_ERROR, 1, "LNCunlink( %s ) -> findNetworkPlugin( %s )", URL, protocol );
		goto out;
	}

	if ( getDepth( URL ) < plugin->minimumTouchDepth ) {
		putNetworkPlugin( plugin );
		errno = EACCES;
		goto out;
	}

	if ( !plugin->unlink && !plugin->mount ) {
		putNetworkPlugin( plugin );
		errno = ENOSYS;
		goto out;
	}

	if ( plugin->unlink && ( plugin->type != USE_BROWSELIST ) )
		retval = plugin->unlink( URL );
	else
		retval = DNPunlink( URL, plugin );

	putNetworkPlugin( plugin );

out:
	retval = runLNCunlinkHooks( URL, &replace, AFTER_SYSCALL, retval );

	LNCdebug( LNC_DEBUG, 0, "LNCunlink( %s ) = %d ( %s )", URL, retval, strerror_r( ( retval == 0 ) ? 0 : errno, errorString, BUFSIZ ) );
	pthread_setcancelstate( oldCancelState, NULL );

	return retval;
}

int LNCtruncate( const char *URL, off_t size ) {
	char * protocol;
	char errorString[ BUFSIZ ];
	char buffer[ NAME_MAX + 1 ];
	int retval;
	int replace;
	int oldCancelState;
	NetworkPlugin *plugin;

	pthread_setcancelstate( PTHREAD_CANCEL_DISABLE, &oldCancelState );
	retval = runLNCtruncateHooks( URL, size, &replace, PRE_SYSCALL, -EXIT_FAILURE );
	if ( replace )
		goto out;

	if ( strcmp( URL, "network://" ) == 0 ) {
		errno = EISDIR;
		goto out;
	}

	protocol = getProtocolType( URL, buffer, NAME_MAX + 1 );
	if ( !protocol ) {
		LNCdebug( LNC_ERROR, 1, "LNCtruncate( %s ) -> getProtocolType()", URL );
		goto out;
	}

	plugin = findNetworkPlugin( protocol );
	if ( !plugin ) {
		LNCdebug( LNC_ERROR, 1, "LNCtruncate( %s ) -> findNetworkPlugin( %s )", URL, protocol );
		goto out;
	}

	if ( getDepth( URL ) < plugin->minimumTouchDepth ) {
		putNetworkPlugin( plugin );
		errno = EACCES;
		goto out;
	}

	if ( !plugin->truncate && !plugin->mount ) {
		putNetworkPlugin( plugin );
		errno = ENOSYS;
		goto out;
	}

	if ( plugin->truncate && ( plugin->type != USE_BROWSELIST ) )
		retval = plugin->truncate( URL, size );
	else
		retval = DNPtruncate( URL, size, plugin );

	putNetworkPlugin( plugin );

out:
	retval = runLNCtruncateHooks( URL, size, &replace, AFTER_SYSCALL, retval );

	LNCdebug( LNC_DEBUG, 0, "LNCtruncate( %s, %lld ) = %d ( %s )", URL, size, retval, strerror_r( ( retval == 0 ) ? 0 : errno, errorString, BUFSIZ ) );
	pthread_setcancelstate( oldCancelState, NULL );

	return retval;
}

int LNCrmdir( const char *URL ) {
	char * protocol;
	char errorString[ BUFSIZ ];
	char buffer[ NAME_MAX + 1 ];
	int retval;
	int replace;
	int oldCancelState;
	NetworkPlugin *plugin;

	pthread_setcancelstate( PTHREAD_CANCEL_DISABLE, &oldCancelState );
	retval = runLNCrmdirHooks( URL, &replace, PRE_SYSCALL, -EXIT_FAILURE );
	if ( replace )
		goto out;

	if ( strcmp( URL, "network://" ) == 0 ) {
		errno = EACCES;
		goto out;
	}

	protocol = getProtocolType( URL, buffer, NAME_MAX + 1 );
	if ( !protocol ) {
		LNCdebug( LNC_ERROR, 1, "LNCrmdir( %s ) -> getProtocolType()", URL );
		goto out;
	}

	plugin = findNetworkPlugin( protocol );
	if ( !plugin ) {
		LNCdebug( LNC_ERROR, 1, "LNCrmdir( %s ) -> findNetworkPlugin( %s )", URL, protocol );
		goto out;
	}

	if ( getDepth( URL ) < plugin->minimumTouchDepth ) {
		putNetworkPlugin( plugin );
		errno = EACCES;
		goto out;
	}

	if ( !plugin->rmdir && !plugin->mount ) {
		putNetworkPlugin( plugin );
		errno = ENOSYS;
		goto out;
	}

	if ( plugin->rmdir && ( plugin->type != USE_BROWSELIST ) )
		retval = plugin->rmdir( URL );
	else
		retval = DNPrmdir( URL, plugin );

	putNetworkPlugin( plugin );

out:
	retval = runLNCrmdirHooks( URL, &replace, AFTER_SYSCALL, retval );

	LNCdebug( LNC_DEBUG, 0, "LNCrmdir( %s ) = %d ( %s )", URL, retval, strerror_r( ( retval == 0 ) ? 0 : errno, errorString, BUFSIZ ) );
	pthread_setcancelstate( oldCancelState, NULL );

	return retval;
}

int LNCmkdir( const char *URL, mode_t mode ) {
	char * protocol;
	char errorString[ BUFSIZ ];
	char buffer[ NAME_MAX + 1 ];
	int retval;
	int replace;
	int oldCancelState;
	NetworkPlugin *plugin;

	pthread_setcancelstate( PTHREAD_CANCEL_DISABLE, &oldCancelState );
	retval = runLNCmkdirHooks( URL, mode, &replace, PRE_SYSCALL, -EXIT_FAILURE );
	if ( replace )
		goto out;

	if ( strcmp( URL, "network://" ) == 0 ) {
		errno = EEXIST;
		goto out;
	}

	protocol = getProtocolType( URL, buffer, NAME_MAX + 1 );
	if ( !protocol ) {
		LNCdebug( LNC_ERROR, 1, "LNCmkdir( %s ) -> getProtocolType()", URL );
		goto out;
	}

	plugin = findNetworkPlugin( protocol );
	if ( !plugin ) {
		LNCdebug( LNC_ERROR, 1, "LNCmkdir( %s ) -> findNetworkPlugin( %s )", URL, protocol );
		goto out;
	}

	if ( getDepth( URL ) < plugin->minimumTouchDepth ) {
		putNetworkPlugin( plugin );
		errno = EACCES;
		goto out;
	}

	if ( !plugin->mkdir && !plugin->mount ) {
		putNetworkPlugin( plugin );
		errno = ENOSYS;
		goto out;
	}

	if ( plugin->mkdir && ( plugin->type != USE_BROWSELIST ) )
		retval = plugin->mkdir( URL, mode );
	else
		retval = DNPmkdir( URL, mode, plugin );

	putNetworkPlugin( plugin );

out:
	retval = runLNCmkdirHooks( URL, mode, &replace, AFTER_SYSCALL, retval );

	LNCdebug( LNC_DEBUG, 0, "LNCmkdir( %s, %d ) = %d ( %s )", URL, mode, retval, strerror_r( ( retval == 0 ) ? 0 : errno, errorString, BUFSIZ ) );
	pthread_setcancelstate( oldCancelState, NULL );

	return retval;
}

int LNCrename( const char *fromURL, const char *toURL ) {
	char * protocol;
	char errorString[ BUFSIZ ];
	char buffer[ NAME_MAX + 1 ];
	int retval;
	int replace;
	int oldCancelState;
	NetworkPlugin *pluginFrom = NULL;
	NetworkPlugin *pluginTo = NULL;

	pthread_setcancelstate( PTHREAD_CANCEL_DISABLE, &oldCancelState );
	retval = runLNCrenameHooks( fromURL, toURL, &replace, PRE_SYSCALL, -EXIT_FAILURE );
	if ( replace )
		goto out;

	if ( strcmp( fromURL, "network://" ) == 0 || strcmp( toURL, "network://" ) == 0 ) {
		errno = EACCES;
		goto out;
	}

	protocol = getProtocolType( fromURL, buffer, NAME_MAX + 1 );
	if ( !protocol ) {
		LNCdebug( LNC_ERROR, 1, "LNCrename( %s, %s ) -> getProtocolType()", fromURL, toURL );
		goto out;
	}

	pluginFrom = findNetworkPlugin( protocol );
	if ( !pluginFrom ) {
		LNCdebug( LNC_ERROR, 1, "LNCrename( %s, %s ) -> findNetworkPlugin( %s )", fromURL, toURL, protocol );
		goto out;
	}

	if ( getDepth( fromURL ) < pluginFrom->minimumTouchDepth ) {
		putNetworkPlugin( pluginFrom );
		errno = EACCES;
		goto out;
	}

	if ( !pluginFrom->rename && !pluginFrom->mount ) {
		putNetworkPlugin( pluginFrom );
		errno = ENOSYS;
		goto out;
	}

	protocol = getProtocolType( toURL, buffer, NAME_MAX + 1 );
	if ( !protocol ) {
		LNCdebug( LNC_ERROR, 1, "LNCrename( %s, %s ) -> getProtocolType()", fromURL, toURL );
		putNetworkPlugin( pluginFrom );
		goto out;
	}

	pluginTo = findNetworkPlugin( protocol );
	if ( !pluginTo ) {
		LNCdebug( LNC_ERROR, 1, "LNCrename( %s, %s ) -> findNetworkPlugin( %s )", fromURL, toURL, protocol );
		putNetworkPlugin( pluginFrom );
		goto out;
	}

	if ( getDepth( toURL ) < pluginTo->minimumTouchDepth ) {
		putNetworkPlugin( pluginFrom );
		putNetworkPlugin( pluginTo );
		errno = EACCES;
		goto out;
	}

	if ( pluginFrom != pluginTo ) {
		putNetworkPlugin( pluginFrom );
		putNetworkPlugin( pluginTo );
		errno = EXDEV;
		goto out;
	}

	if ( !pluginFrom->rename && !pluginFrom->mount ) {
		putNetworkPlugin( pluginFrom );
		putNetworkPlugin( pluginTo );
		errno = ENOSYS;
		goto out;
	}

	if ( pluginFrom->rename && ( pluginFrom->type != USE_BROWSELIST ) )
		retval = pluginFrom->rename( fromURL, toURL );
	else
		retval = DNPrename( fromURL, toURL, pluginFrom );

	putNetworkPlugin( pluginFrom );
	putNetworkPlugin( pluginTo );

out:
	retval = runLNCrenameHooks( fromURL, toURL, &replace, AFTER_SYSCALL, retval );

	LNCdebug( LNC_DEBUG, 0, "LNCrename( %s, %s ) = %d ( %s )", fromURL, toURL, retval, strerror_r( ( retval == 0 ) ? 0 : errno, errorString, BUFSIZ ) );
	pthread_setcancelstate( oldCancelState, NULL );

	return retval;
}

int LNCsymlink( const char *fromURL, const char *toURL ) {
	char * protocol;
	char errorString[ BUFSIZ ];
	char buffer[ NAME_MAX + 1 ];
	int retval;
	int replace;
	int oldCancelState;
	NetworkPlugin *pluginFrom = NULL;
	NetworkPlugin *pluginTo = NULL;

	pthread_setcancelstate( PTHREAD_CANCEL_DISABLE, &oldCancelState );
	retval = runLNCsymlinkHooks( fromURL, toURL, &replace, PRE_SYSCALL, -EXIT_FAILURE );
	if ( replace )
		goto out;

	if ( strcmp( fromURL, "network://" ) == 0 || strcmp( toURL, "network://" ) == 0 ) {
		errno = EACCES;
		goto out;
	}

	protocol = getProtocolType( fromURL, buffer, NAME_MAX + 1 );
	if ( !protocol ) {
		LNCdebug( LNC_ERROR, 1, "LNCsymlink( %s, %s ) -> getProtocolType()", fromURL, toURL );
		goto out;
	}

	pluginFrom = findNetworkPlugin( protocol );
	if ( !pluginFrom ) {
		LNCdebug( LNC_ERROR, 1, "LNCsymlink( %s, %s ) -> findNetworkPlugin( %s )", fromURL, toURL, protocol );
		goto out;
	}

	if ( getDepth( fromURL ) < pluginFrom->minimumTouchDepth ) {
		putNetworkPlugin( pluginFrom );
		errno = EACCES;
		goto out;
	}

	protocol = getProtocolType( toURL, buffer, NAME_MAX + 1 );
	if ( !protocol ) {
		LNCdebug( LNC_ERROR, 1, "LNCrename( %s, %s ) -> getProtocolType()", fromURL, toURL );
		putNetworkPlugin( pluginFrom );
		goto out;
	}

	pluginTo = findNetworkPlugin( protocol );
	if ( !pluginFrom ) {
		LNCdebug( LNC_ERROR, 1, "LNCrename( %s, %s ) -> findNetworkPlugin( %s )", fromURL, toURL, protocol );
		putNetworkPlugin( pluginFrom );
		goto out;
	}

	if ( getDepth( toURL ) < pluginTo->minimumTouchDepth ) {
		putNetworkPlugin( pluginFrom );
		putNetworkPlugin( pluginTo );
		errno = EACCES;
		goto out;
	}


	if ( pluginFrom != pluginTo ) {
		putNetworkPlugin( pluginFrom );
		putNetworkPlugin( pluginTo );
		errno = EXDEV;
		goto out;
	}

	if ( !pluginFrom->symlink && !pluginFrom->mount ) {
		putNetworkPlugin( pluginFrom );
		putNetworkPlugin( pluginTo );
		errno = ENOSYS;
		goto out;
	}

	if ( pluginFrom->symlink && ( pluginFrom->type != USE_BROWSELIST ) )
		retval = pluginFrom->symlink( fromURL, toURL );
	else
		retval = DNPsymlink( fromURL, toURL, pluginFrom );

	putNetworkPlugin( pluginFrom );
	putNetworkPlugin( pluginTo );

out:
	retval = runLNCsymlinkHooks( fromURL, toURL, &replace, PRE_SYSCALL, retval );

	LNCdebug( LNC_DEBUG, 0, "LNCsymlink( %s, %s ) = %d ( %s )", fromURL, toURL, retval, strerror_r( ( retval == 0 ) ? 0 : errno, errorString, BUFSIZ ) );
	pthread_setcancelstate( oldCancelState, NULL );

	return retval;
}

/* int LNClink( const char *fromURL, const char *toURL ) {
	char * protocol;
	char errorString[ BUFSIZ ];
	char buffer[ NAME_MAX + 1 ];
	NetworkPlugin *pluginFrom = NULL;
	NetworkPlugin *pluginTo = NULL;
	int retval;

	if ( getDepth( fromURL ) < 5 || getDepth( toURL ) < 5 ) {
		errno = EACCES;
		goto out;
	}

	protocol = getProtocolType( fromURL, buffer );
	if ( !protocol ) {
		LNCdebug( LNC_ERROR, 1, "LNClink( %s, %s ) -> getProtocolType()", fromURL, toURL );
		goto out;
	}

	pluginFrom = findNetworkPlugin( protocol );
	if ( !pluginFrom ) {
		LNCdebug( LNC_ERROR, 1, "LNClink( %s, %s ) -> findNetworkPlugin( %s )", fromURL, toURL, protocol );
		goto out;
	}

	protocol = getProtocolType( toURL, buffer );
	if ( !protocol ) {
		LNCdebug( LNC_ERROR, 1, "LNCrename( %s, %s ) -> getProtocolType()", fromURL, toURL );
		goto out;
	}

	pluginTo = findNetworkPlugin( protocol );
	if ( !pluginFrom ) {
		LNCdebug( LNC_ERROR, 1, "LNCrename( %s, %s ) -> findNetworkPlugin( %s )", fromURL, toURL, protocol );
		goto out;
	}

	if ( pluginFrom != pluginTo ) {
		errno = EXDEV;
		goto out;
	}

	if ( !pluginFrom->link && !plugin->mount ) {
		errno = ENOSYS;
		goto out;
	}

	if ( plugin->link )
		retval = pluginFrom->link( fromURL, toURL );
	else
		retval = DNPlink( fromURL, toURL );

out:
	if ( pluginFrom )
		putNetworkPlugin( pluginFrom );
	if ( pluginTo )
		putNetworkPlugin( pluginTo );

	LNCdebug( LNC_DEBUG, 0, "LNClink( %s, %s ) = %d ( %s )", fromURL, toURL, retval, strerror_r( ( retval == 0 ) ? 0 : errno, errorString, BUFSIZ ) );
	return retval;
}*/


int LNCchown( const char *URL, uid_t owner, gid_t group ) {
	char * protocol;
	char errorString[ BUFSIZ ];
	char buffer[ NAME_MAX + 1 ];
	int retval;
	int replace;
	int oldCancelState;
	NetworkPlugin *plugin;

	pthread_setcancelstate( PTHREAD_CANCEL_DISABLE, &oldCancelState );
	retval = runLNCchownHooks( URL, owner, group, &replace, PRE_SYSCALL, -EXIT_FAILURE );
	if ( replace )
		goto out;

	if ( strcmp( URL, "network://" ) == 0 ) {
		errno = EACCES;
		goto out;
	}

	protocol = getProtocolType( URL, buffer, NAME_MAX + 1 );
	if ( !protocol ) {
		LNCdebug( LNC_ERROR, 1, "LNCchown( %s ) -> getProtocolType()", URL );
		goto out;
	}

	plugin = findNetworkPlugin( protocol );
	if ( !plugin ) {
		LNCdebug( LNC_ERROR, 1, "LNCchown( %s ) -> findNetworkPlugin( %s )", URL, protocol );
		goto out;
	}

	if ( getDepth( URL ) < plugin->minimumTouchDepth ) {
		putNetworkPlugin( plugin );
		errno = EACCES;
		goto out;
	}

	if ( !plugin->chown && !plugin->mount ) {
		putNetworkPlugin( plugin );
		errno = ENOSYS;
		goto out;
	}

	if ( plugin->chown && ( plugin->type != USE_BROWSELIST ) )
		retval = plugin->chown( URL, owner, group );
	else
		retval = DNPchown( URL, owner, group, plugin );

	putNetworkPlugin( plugin );

out:
	retval = runLNCchownHooks( URL, owner, group, &replace, AFTER_SYSCALL, retval );

	LNCdebug( LNC_DEBUG, 0, "LNCchown( %s, %d, %d ) = %d ( %s )", URL, owner, group, retval, strerror_r( ( retval == 0 ) ? 0 : errno, errorString, BUFSIZ ) );
	pthread_setcancelstate( oldCancelState, NULL );

	return retval;
}

int LNCchmod( const char *URL, mode_t mode ) {
	char * protocol;
	char errorString[ BUFSIZ ];
	char buffer[ NAME_MAX + 1 ];
	int retval;
	int replace;
	int oldCancelState;
	NetworkPlugin *plugin;

	pthread_setcancelstate( PTHREAD_CANCEL_DISABLE, &oldCancelState );
	retval = runLNCchmodHooks( URL, mode, &replace, PRE_SYSCALL, -EXIT_FAILURE );
	if ( replace )
		goto out;

	if ( strcmp( URL, "network://" ) == 0 ) {
		errno = EACCES;
		goto out;
	}

	protocol = getProtocolType( URL, buffer, NAME_MAX + 1 );
	if ( !protocol ) {
		LNCdebug( LNC_ERROR, 1, "LNCchmod( %s ) -> getProtocolType()", URL );
		goto out;
	}

	plugin = findNetworkPlugin( protocol );
	if ( !plugin ) {
		LNCdebug( LNC_ERROR, 1, "LNCchmod( %s ) -> findNetworkPlugin( %s )", URL, protocol );
		goto out;
	}

	if ( getDepth( URL ) < plugin->minimumTouchDepth ) {
		putNetworkPlugin( plugin );
		errno = EACCES;
		goto out;
	}

	if ( !plugin->chmod && !plugin->mount ) {
		putNetworkPlugin( plugin );
		errno = ENOSYS;
		goto out;
	}

	if ( plugin->chmod && ( plugin->type != USE_BROWSELIST ) )
		retval = plugin->chmod( URL, mode );
	else
		retval = DNPchmod( URL, mode, plugin );

	putNetworkPlugin( plugin );

out:
	retval = runLNCchmodHooks( URL, mode, &replace, AFTER_SYSCALL, retval );

	LNCdebug( LNC_DEBUG, 0, "LNCchmod( %s, %d ) = %d ( %s )", URL, mode, retval, strerror_r( ( retval == 0 ) ? 0 : errno, errorString, BUFSIZ ) );
	pthread_setcancelstate( oldCancelState, NULL );

	return retval;
}

int LNCutime( const char *URL, struct utimbuf *buf ) {
	char *protocol;
	char errorString[ BUFSIZ ];
	char buffer[ NAME_MAX + 1 ];
	int retval;
	int replace;
	int oldCancelState;
	NetworkPlugin *plugin;

	pthread_setcancelstate( PTHREAD_CANCEL_DISABLE, &oldCancelState );
	retval = runLNCutimeHooks( URL, buf, &replace, PRE_SYSCALL, -EXIT_FAILURE );
	if ( replace )
		goto out;

	if ( strcmp( URL, "network://" ) == 0 ) {
		errno = EACCES;
		goto out;
	}

	protocol = getProtocolType( URL, buffer, NAME_MAX + 1 );
	if ( !protocol ) {
		LNCdebug( LNC_ERROR, 1, "LNCutime( %s ) -> getProtocolType()", URL );
		goto out;
	}

	plugin = findNetworkPlugin( protocol );
	if ( !plugin ) {
		LNCdebug( LNC_ERROR, 1, "LNCutime( %s ) -> findNetworkPlugin( %s )", URL, protocol );
		goto out;
	}

	if ( getDepth( URL ) < plugin->minimumTouchDepth ) {
		putNetworkPlugin( plugin );
		errno = EACCES;
		goto out;
	}

	if ( !plugin->utime && !plugin->mount ) {
		putNetworkPlugin( plugin );
		errno = ENOSYS;
		goto out;
	}

	if ( plugin->utime && ( plugin->type != USE_BROWSELIST ) )
		retval = plugin->utime( URL, buf );
	else
		retval = DNPutime( URL, buf, plugin );

	putNetworkPlugin( plugin );

out:
	retval = runLNCutimeHooks( URL, buf, &replace, AFTER_SYSCALL, retval );

	LNCdebug( LNC_DEBUG, 0, "LNCutime( %s, buf ) = %d ( %s )", URL, retval, strerror_r( ( retval == 0 ) ? 0 : errno, errorString, BUFSIZ ) );
	pthread_setcancelstate( oldCancelState, NULL );

	return retval;
}
