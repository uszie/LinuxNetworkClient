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
#include "pluginhelper.h"
#include "util.h"
#include "user.h"
#include "dir.h"
#include "plugin.h"

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <limits.h>
#include <sys/stat.h>
#include <kdb.h>
//#include <kdbbackend.h>


struct networkConfig networkConfig =
{
	.LogLevel = LNC_INFO,
	.DisabledPlugins = NULL,
 	.DefaultUsername = NULL,
	.DefaultPassword = NULL,
	.PingNetworks = NULL,
 	.IgnoreAddresses = NULL,
  	.UseBroadcast = 0,
 	.DisablePasswordWriting = 0,
	.BrowselistUpdateInterval = 10,
	.UmountInterval = 60,
 	.EnableDebugMode = 0,
  	.UseFullNetworkName = 0,
};

pthread_rwlock_t LNCConfigLock = PTHREAD_RWLOCK_INITIALIZER;
pthread_once_t LNCConfigLockInit = PTHREAD_ONCE_INIT;

char *removeTrailingWhites ( char *string )
{
	int len;

	len = strlen ( string ) - 1;
	if ( len <= 0 )
		return string;

	for ( len = len; string[ len ] == ' '; len-- )
		;

	string[ len + 1 ] = '\0';
	return string;
}

char *removePreceedingWhites ( char *string )
{
	while ( *string == ' ' )
		++string;

	return string;
}

char *removeWhites ( char *string )
{
	char * newString;

	newString = removePreceedingWhites ( string );
	if ( !newString )
		return NULL;

	return removeTrailingWhites ( newString );
}

char *removeTrailingSlashes ( char *string )
{
	int len;

	for ( len = strlen ( string ) - 1; string[ len ] == '/'; len-- )
		;

	string[ len + 1 ] = '\0';
	return string;
}

ssize_t copyFileToBuffer ( char **string, const char *filename )
{
	char *buffer;
	char *oldBuffer;
	char *ptr;
	int fd;
	ssize_t readSize;
	size_t requestSize;
	size_t totalSize;
	size_t allocSize;

	fd = open ( filename, O_RDONLY );
	if ( fd < 0 )
	{
		LNCdebug ( LNC_ERROR, 1, "copyFileToBuffer(%s) -> open(%s, %s )", filename, filename, "O_RDONLY" );
		return -EXIT_FAILURE;
	}

	allocSize = 8192;
	buffer = realloc( NULL, allocSize );
	if ( !buffer ) {
		LNCdebug( LNC_ERROR, 1, "copyFileToBuffer( %s ) -> malloc( %d )", filename, allocSize );
		close ( fd );
		return -EXIT_FAILURE;
	}

	ptr = buffer;
	readSize = 0;
	totalSize = 0;
	requestSize = 4096;
	while ( ( readSize = read ( fd, ptr, requestSize ) ) > 0  ) {
		totalSize = readSize + totalSize;
		if ( ( totalSize + requestSize ) > allocSize ) {
			oldBuffer = buffer;
			allocSize = allocSize + ( 10 * requestSize );
			buffer = realloc( buffer, allocSize );
			if ( !buffer ) {
				LNCdebug( LNC_ERROR, 1, "copyFileToBuffer( %s ) -> realloc( %d )", filename, allocSize );
				free( oldBuffer );
				close ( fd );
				return -EXIT_FAILURE;
			}
		}
		ptr = buffer+totalSize;
	}

	close ( fd );

	if ( readSize < 0 ) {
		LNCdebug( LNC_ERROR, 1, "copyFileToBuffer( %s ) -> read( %d, %d )", filename, fd, requestSize );
		free( buffer );
		return -EXIT_FAILURE;
	}

	buffer[ totalSize ] = '\0';
	*string = buffer;

	return totalSize;
}

int copyBufferToFile ( const char *string, const char *filename )
{
	int fd;
	ssize_t write_size;
	ssize_t string_size;
	char cpy[ PATH_MAX ];
	char *ptr;

openFile:
	fd = open ( filename, O_RDWR | O_CREAT | O_TRUNC );
	if ( fd < 0 )
	{
		if ( errno == ENOENT )
		{
			strlcpy ( cpy, filename, PATH_MAX );
			ptr = strrchr ( cpy, '/' );
			if ( ptr )
			{
				*ptr = '\0';
				buildPath ( cpy, 0755 );
				goto openFile;
			}
		}
		LNCdebug ( LNC_ERROR, 1, "copyBufferToFile() -> open(%s, %s)", filename, "O_RDWR | O_CREAT" );
		return -EXIT_FAILURE;
	}

	string_size = strlen( string );
	write_size = write ( fd, string, string_size );
	close ( fd );
	if ( write_size < 0 )
	{
		LNCdebug ( LNC_ERROR, 1, "copyBufferToFile() -> write()" );
		return -EXIT_FAILURE;
	}
	else if ( write_size != string_size )
		LNCdebug ( LNC_ERROR, 0, "copyBufferToFile() -> write(): Error during write" );

	return EXIT_SUCCESS;
}

static char *rm_newline ( char *string )
{
	int i;

	for ( i = strlen ( string ) - 1; string[ i ] == '\n' || string[ i ] == ' ' || string[ i ] == '\t' || string[ i ] == '\r'; --i )
		;
	string[ i + 1 ] = '\0';

	for ( i = 0; string[ i ] == '\n' || string[ i ] == ' ' || string[ i ] == '\t' || string[ i ] == '\r'; ++i )
		;

	return string + i;
}

static char *search_line ( const char *string, const char *line, const char *group )
{
	char * start, *end;
	char *result;

	if ( group )
	{
		start = strstr ( string, group );
		if ( !start ||
		        start[ strlen ( group ) ] != ']' )
			return NULL;

		end = strstr ( start, "[" );
		if ( !end )
			end = ( char * ) string + strlen ( string );
	}
	else
	{
		start = ( char * ) string;
		end = ( char * ) string + strlen ( string );
	}

	result = strstr ( start, line );
	if ( result && result < end )
		return result;

	return NULL;
}

static char *add_line ( const char *string, const char *line, const char *group )
{
	int group_len = strlen ( group );
	int line_len = strlen ( line );
	int string_len = strlen ( string );
	int end_len;
	char *start, *end;
	char *ptr;
	char *result;
	char *string_cpy = strdup ( string );

	ptr = strstr ( string_cpy, group );
	if ( !ptr || * ( ptr + group_len ) != ']' )
	{
		result = malloc ( string_len + line_len + group_len + 7 );
		result[ 0 ] = '\0';

		if ( string_len == 0 )
			sprintf ( result, "[%s]\n%s\n", group, line );
		else
			sprintf ( result, "%s\n\n[%s]\n%s\n", rm_newline ( string_cpy ), group, line );

		free ( string_cpy );
		return result;
	}

	start = string_cpy;
	end = strstr ( ptr, "[" );
	if ( !end )
		end = string_cpy + string_len;
	else
		* ( end - 1 ) = '\0';

	start = rm_newline ( start );
	end = rm_newline ( end );

	result = malloc ( strlen ( start ) + line_len + ( end_len = strlen ( end ) ) + 3 );
	result[ 0 ] = '\0';

	if ( end_len == 0 )
		sprintf ( result, "%s\n%s\n", start, line );
	else
		sprintf ( result, "%s\n%s\n\n%s", start, line, end );

	free ( string_cpy );

	return result;
}

static char *replace_value ( const char *string, const char *addr, const char *value )
{
	char * end = NULL;
	char *ptr = NULL;
	char *result = NULL;
	int i, len;

	for ( i = 0; addr[ i ] != '\0'; i++ )
	{
		if ( addr[ i ] == '=' )                     /* || addr[i] == ' ' )*/
			ptr = ( char * ) addr + ( i + 1 );

		if ( addr[ i ] == '\n' || addr[ i + 1 ] == '\0' )
		{
			end = ( char * ) addr + ( i + 1 );
			break;
		}
	}

	result = malloc ( ( ptr - string ) + strlen ( value ) + strlen ( end ) + 2 );
	if ( !result )
		return NULL;

	result[ 0 ] = '\0';
	len = ptr - string;

	strlcpy ( result, string, len );
	strcat ( result, value );
	strcat ( result, "\n" );
	strcat ( result, end );

	return result;
}

/*int write_entry( const char *filename, const char *key, const char *value, const char *group )
{
	int retval, len;
	char *string = NULL;
	char *result = NULL;
	char *addr = NULL;
	char *line = NULL;
	char val[ BUFSIZ ];

	if ( !value )
		strcpy( val, "" );
	else {
		if ( strlen( value ) >= BUFSIZ )
			return -E2BIG;
		strcpy( val, value );
	}

	len = copyFileToBuffer( &string, filename );
	if ( len < 0 )
		return len;

	addr = search_line( string, key, group );
	if ( addr )
		result = replace_value( string, addr, val );
	else {
		line = malloc( strlen( key ) + strlen( val ) + 2 );
		sprintf( line, "%s=%s", key, val );
		result = add_line( string, line, group );
	}

	free( string );
	retval = copyBufferToFile( result, filename );
	free( result );
	if ( retval < 0 )
		return retval;

	return EXIT_SUCCESS;
}*/

int writeCharEntry ( const char *filename, const char *key, const char *value, const char *group )
{
	int retval, len;
	char *string = NULL;
	char *result = NULL;
	char *addr = NULL;
	char *line = NULL;
	char val[ BUFSIZ ];

	if ( !value )
		strcpy ( val, "" );
	else
	{
		if ( strlen ( value ) >= BUFSIZ )
		{
			errno = -E2BIG;
			return -EXIT_FAILURE;
		}
		strlcpy ( val, value, BUFSIZ );
	}

	len = copyFileToBuffer ( &string, filename );
	if ( len < 0 )
		return -EXIT_FAILURE;

	addr = search_line ( string, key, group );
	if ( addr )
		result = replace_value ( string, addr, val );
	else
	{
		line = malloc ( strlen ( key ) + strlen ( val ) + 2 );
		sprintf ( line, "%s=%s", key, val );
		result = add_line ( string, line, group );
	}

	free ( string );
	retval = copyBufferToFile ( result, filename );
	free ( result );

	return retval;
}

int writeNumEntry ( const char *filename, const char *key, const int value, const char *group )
{
	int retval, len;
	char *string = NULL;
	char *result = NULL;
	char *addr = NULL;
	char *line = NULL;
	char val[ BUFSIZ ];

	len = copyFileToBuffer ( &string, filename );
	if ( len < 0 )
		return -EXIT_FAILURE;

	sprintf ( val, "%d", value );
	addr = search_line ( string, key, group );
	if ( addr )
		result = replace_value ( string, addr, val );
	else
	{
		line = malloc ( strlen ( key ) + strlen ( val ) + 2 );
		sprintf ( line, "%s=%s", key, val );
		result = add_line ( string, line, group );
	}

	free ( string );
	retval = copyBufferToFile ( result, filename );
	free ( result );

	return retval;
}
/*void *read_entry( const char *filename, const char *key, const char *group, const char *def )
{
	char * value = NULL;
	char *string = NULL;
	char *start = NULL;
	char *end = NULL;
	char result[ BUFSIZ ];
	char format[ BUFSIZ ];
	int isInt, len;
	char *remain;

	len = copyFileToBuffer( &string, filename );
	if ( len < 0 )
		return def ? ( ( isInt = atoi( def ) ) ? ( void * ) isInt : ( void* ) strdup( def ) ) : NULL;

	if ( group ) {
		start = strstr( string, group );
		if ( !start ||
		        start[ strlen( group ) ] != ']' ) {
			free( string );
			return def ? ( ( isInt = atoi( def ) ) ? ( void * ) isInt : ( void * ) strdup( def ) ) : NULL;
		}

		end = strstr( start, "[" );
		if ( !end )
			end = string + strlen( string );
	} else {
		start = string;
		end = string + strlen( string );
	}

	value = strstr( start, key );
	if ( value && value < end ) {
		char * ptr;
		result[ 0 ] = format[ 0 ] = '\0';
//		sprintf(format, "%s = %%s", key);
		sprintf( format, "%s = %[^][=]", key );
		if ( ( ptr = strchr( value, '\n' ) ) )
			* ptr = '\0';
		sscanf( value, format, result );
		removeTrailingWhites( result );
		free( string );
		if ( result[ 0 ] != '\0' ) {
			if ( ( isInt = strtol( result, &remain, 10 ) ) && ( *remain == '\0' ) )
				return ( void * ) isInt;
			return ( void * ) strdup( result );
		} else
			return def ? ( ( ( isInt = strtol( def, &remain, 10 ) ) && ( *remain == '\0' ) ) ? ( void * ) isInt : ( void * ) strdup( def ) ) : NULL;
	}

	free( string );
	return def ? ( ( ( isInt = strtol( def, &remain, 10 ) ) && ( *remain == '\0' ) ) ? ( void * ) isInt : ( void * ) strdup( def ) ) : NULL;
}*/

char *readCharEntry ( const char *filename, const char *key, const char *group, const char *def )
{
	char * value = NULL;
	char *string = NULL;
	char *start = NULL;
	char *end = NULL;
	char result[ BUFSIZ ];
	char format[ BUFSIZ ];
	int len;

	len = copyFileToBuffer ( &string, filename );
	if ( len < 0 )
		return def ? strdup ( def ) : NULL;

	if ( group )
	{
		start = strstr ( string, group );
		if ( !start ||
		        start[ strlen ( group ) ] != ']' )
		{
			free ( string );
			return def ? strdup ( def ) : NULL;
		}

		end = strstr ( start, "[" );
		if ( !end )
			end = string + strlen ( string );
	}
	else
	{
		start = string;
		end = string + strlen ( string );
	}

	value = strstr ( start, key );
	if ( value && value < end )
	{
		char * ptr;
		result[ 0 ] = format[ 0 ] = '\0';
		/*		sprintf(format, "%s = %%s", key);*/
		sprintf ( format, "%s = %%[^][=]", key );
		if ( ( ptr = strchr ( value, '\n' ) ) )
			* ptr = '\0';
		sscanf ( value, format, result );
		removeTrailingWhites ( result );
		free ( string );
		if ( result[ 0 ] != '\0' )
		{
			return strdup ( result );
		}
		else
			return def ? strdup ( def ) : NULL;
	}

	free ( string );
	return def ? strdup ( def ) : NULL;
}

int readNumEntry ( const char *filename, const char *key, const char *group, const int def )
{
	char * value = NULL;
	char *string = NULL;
	char *start = NULL;
	char *end = NULL;
	char result[ BUFSIZ ];
	char format[ BUFSIZ ];
	int isInt, len;
	char *remain;

	len = copyFileToBuffer ( &string, filename );
	if ( len < 0 )
		return def;

	if ( group )
	{
		start = strstr ( string, group );
		if ( !start ||
		        start[ strlen ( group ) ] != ']' )
		{
			free ( string );
			return def;
		}

		end = strstr ( start, "[" );
		if ( !end )
			end = string + strlen ( string );
	}
	else
	{
		start = string;
		end = string + strlen ( string );
	}

	value = strstr ( start, key );
	if ( value && value < end )
	{
		char * ptr;
		result[ 0 ] = format[ 0 ] = '\0';
		/*		sprintf(format, "%s = %%s", key);*/
		sprintf ( format, "%s = %%[^][=]", key );
		if ( ( ptr = strchr ( value, '\n' ) ) )
			* ptr = '\0';
		sscanf ( value, format, result );
		removeTrailingWhites ( result );
		free ( string );
		if ( result[ 0 ] != '\0' )
		{
			if ( ( isInt = strtol ( result, &remain, 10 ) ) && ( *remain == '\0' ) )
				return isInt;
			return def;
		}
		else
			return def;
	}

	free ( string );
	return def;
}

void printNetworkConfiguration ( void )
{
	LNCdebug ( LNC_INFO, 0, "LogLevel = \"%d\"", networkConfig.LogLevel );
	LNCdebug ( LNC_INFO, 0, "DisabledPlugins = \"%s\"", networkConfig.DisabledPlugins );
	LNCdebug ( LNC_INFO, 0, "BrowselistUpdateInterval = \"%llu\"", networkConfig.BrowselistUpdateInterval );
	LNCdebug ( LNC_INFO, 0, "UmountInterval = \"%llu\"", networkConfig.UmountInterval );
	LNCdebug ( LNC_INFO, 0, "UseBroadcast = \"%d\"", networkConfig.UseBroadcast );
	LNCdebug ( LNC_INFO, 0, "PingNetworks = %s", networkConfig.PingNetworks );
	LNCdebug ( LNC_INFO, 0, "IgnoreAddresses = %s", networkConfig.IgnoreAddresses );
	LNCdebug ( LNC_INFO, 0, "DefaultUsername = \"%s\"", networkConfig.DefaultUsername );
	LNCdebug ( LNC_INFO, 0, "DefaultPassword = \"%s\"", networkConfig.DefaultPassword );
	LNCdebug ( LNC_INFO, 0, "DisablePasswordWriting = \"%d\"", networkConfig.DisablePasswordWriting );
	LNCdebug ( LNC_INFO, 0, "UseFullNetworkName = \"%d\"", networkConfig.UseFullNetworkName );
	LNCdebug ( LNC_INFO, 0, "EnableDebugMode = \"%d\"", networkConfig.EnableDebugMode );
}

void freeNetworkConfiguration ( void )
{
	LNCdebug ( LNC_INFO, 0, "freeNetworkConfiguration()" );

	if ( networkConfig.DefaultUsername )
		free ( networkConfig.DefaultUsername );

	if ( networkConfig.DefaultPassword )
		free ( networkConfig.DefaultPassword );

	if ( networkConfig.PingNetworks )
		free( networkConfig.PingNetworks );

	if ( networkConfig.IgnoreAddresses )
		free( networkConfig.IgnoreAddresses );

	if ( networkConfig.DisabledPlugins )
		free( networkConfig.DisabledPlugins );

	networkConfig.DefaultUsername = networkConfig.DefaultPassword = networkConfig.PingNetworks = networkConfig.IgnoreAddresses = networkConfig.DisabledPlugins = NULL;
}

int readNetworkConfiguration ( void )
{
	LNCdebug ( LNC_INFO, 0, "readNetworkConfiguration()" );

	initKeys ( "LNC" );
	networkConfig.LogLevel = getNumKey ( "LNC", "LogLevel", LNC_INFO );
	networkConfig.BrowselistUpdateInterval = getNumKey ( "LNC", "BrowselistUpdateInterval", 10 );
	networkConfig.UmountInterval = getNumKey ( "LNC", "UmountInterval", 10 );
	networkConfig.UseBroadcast = getNumKey ( "LNC", "UseBroadcast", 0 );
	networkConfig.PingNetworks = getCharKey( "LNC", "PingNetworks", "ALL" );
	networkConfig.IgnoreAddresses = getCharKey( "LNC", "IgnoreAddresses", "" );
	networkConfig.DisabledPlugins = getCharKey ( "LNC", "DisabledPlugins", "" );
	networkConfig.DefaultUsername = getCharKey ( "LNC", "DefaultUsername", NULL );
	networkConfig.DefaultPassword = getCharKey ( "LNC", "DefaultPassword", NULL );
	networkConfig.DisablePasswordWriting = getNumKey ( "LNC", "DisablePasswordWriting", 1 );
	networkConfig.UseFullNetworkName = getNumKey ( "LNC", "UseFullNetworkName", 0 );
	networkConfig.EnableDebugMode = getNumKey ( "LNC", "EnableDebugMode", 0 );
	cleanupKeys ( "LNC" );

	return EXIT_SUCCESS;
}


int updateNetworkConfiguration ( const char *path, const char *filename, int event )
{
	static u_int64_t lastTime = 0ULL;
	char fullPath[ PATH_MAX ];
	int retval;
	struct stat stbuf;
	u_int64_t currentTime;

	LNCdebug ( LNC_DEBUG, 0, "updateNetworkConfiguration(): path = %s, filename = %s, event = %d", path, filename, event );
	if ( event != FAMChanged )
		return EXIT_SUCCESS;

	snprintf ( fullPath, PATH_MAX, "%s/%s", path, filename );
	retval = stat ( fullPath, &stbuf );
	if ( retval < 0 )
		LNCdebug ( LNC_ERROR, 1, "updateNetworkConfiguration() -> stat( %s )", fullPath );
	else if ( ( stbuf.st_mode & S_IFMT ) == S_IFDIR )
		return EXIT_SUCCESS;

	currentTime = getCurrentTime();
	if ( ( lastTime + 500000ULL ) > currentTime )
		return EXIT_SUCCESS;

	lastTime = currentTime;

	lockLNCConfig ( WRITE_LOCK );
	freeNetworkConfiguration();
	readNetworkConfiguration();
	printNetworkConfiguration();
	unlockLNCConfig();

	reloadPlugins();
	restartServiceTimer();

	return EXIT_SUCCESS;
}

void *keysDBHandle = NULL;

struct appKey
{
	struct list list;
	char name[ NAME_MAX + 1 ];
	KeySet *userConfig;
	KeySet *systemConfig;
	Key *userRoot;
	Key *systemRoot;
//	KDB *handle;
};

defineList ( keysList, struct appKey );
struct keysList __keysList__;
struct keysList *keysList = &__keysList__;

static int keysCompare ( struct appKey *new, struct appKey *entry )
{
	return strcmp ( new->name, entry->name );
}

static int keysFind ( char *search, struct appKey *entry )
{
	return strncmp ( search, entry->name, strlen ( entry->name ) );
}

static void keysPrint ( struct appKey *entry )
{
	printf ( "%s\n", entry->name );
	LNCdebug ( LNC_INFO, 0, "%s", entry->name );
}

static void initKeyList ( void )
{
	initList ( keysList, keysCompare, keysFind, NULL, keysPrint, NULL, NULL, NULL );
}

static void initKeysDBHandle( void ) {
	if ( !keysDBHandle ) {
		keysDBHandle = ( void * ) kdbOpen();
		if ( !keysDBHandle )
			LNCdebug( LNC_ERROR, 1, "initKeysDBHandle() -> kdbOpen()" );
	}
}

pthread_once_t keyListInit = PTHREAD_ONCE_INIT;
pthread_once_t keysDBHandleInit = PTHREAD_ONCE_INIT;

void clearKeys ( const char *applicationName, int mode )
{
	int retval;
	char configRoot[ PATH_MAX ];
	char keyName[ 200 ];
	char buffer[ BUFSIZ ];
	struct passwd *user;
	KeySet *config;
	Key *current;
	Key *root;

	if ( mode == USER_ROOT )
	{
		user = getSessionInfoByUID ( geteuid(), buffer );
		if ( !user )
		{
			LNCdebug ( LNC_ERROR, 1, "clearKeys( %s ) -> getSessionInfoByUID( %d )", applicationName, geteuid() );
			return ;
		}
		sprintf ( configRoot, "user:%s/sw/%s", user->pw_name, applicationName );
	}
	else
		sprintf ( configRoot, "system/sw/%s", applicationName );


	config = ksNew ( 0 );
	root = keyNew ( configRoot, KEY_END );

	retval = kdbGet ( ( KDB *) keysDBHandle, config, root, 0 );
	ksRewind ( config );
	while ( ( current = ksNext ( config ) ) )
	{
		keyGetFullName ( current, keyName, sizeof ( keyName ) );
		kdbRemove ( ( KDB *) keysDBHandle, keyName );
	}

	keyDel ( root );
	ksDel ( config );
}

void initKeys ( const char *applicationName )
{
	int retval;
	char userConfigRoot[ PATH_MAX ];
	char systemConfigRoot[ PATH_MAX ];
	char buffer[ BUFSIZ ];
	struct appKey *new;
	struct passwd *user;

	pthread_once ( &keyListInit, initKeyList );
	pthread_once ( &keysDBHandleInit, initKeysDBHandle );

	lockList( keysList, WRITE_LOCK );
	user = getSessionInfoByUID ( getuid(), buffer );
	if ( !user )
	{
		LNCdebug ( LNC_ERROR, 1, "initKeys( %s ) -> getSessionInfoByUID( %d )", applicationName, geteuid() );
		unlockList( keysList );
		return ;
	}

	new = malloc ( sizeof ( struct appKey ) );
	if ( !new )
	{
		LNCdebug ( LNC_ERROR, 1, "initKeys( %s ) -> malloc( %d )", applicationName, sizeof ( struct appKey ) );
		unlockList( keysList );
		return ;
	}

	new->userConfig = ksNew ( 0 );
	new->systemConfig = ksNew ( 0 );

	if ( !new->userConfig || !new->systemConfig ) {
		LNCdebug( LNC_ERROR, 1, "initKeys( %s ) -> ksNew( 0 )", applicationName );
		free( new );
		unlockList( keysList );
		return;
	}

	snprintf ( userConfigRoot, PATH_MAX, "user:%s/sw/%s", user->pw_name, applicationName );
	snprintf ( systemConfigRoot, PATH_MAX, "system/sw/%s", applicationName );

	new->userRoot = keyNew ( userConfigRoot, KEY_END );
	if ( !new->userRoot ) {
		LNCdebug( LNC_ERROR, 1, "initKeys( %s ) -> keyNew( %s )", applicationName, userConfigRoot );
		ksDel ( new->userConfig );
		ksDel ( new->systemConfig );
		free( new );
		unlockList( keysList );
		return;
	}

	new->systemRoot = keyNew ( systemConfigRoot, KEY_END );
	if ( !new->systemRoot ) {
		LNCdebug( LNC_ERROR, 1, "initKeys( %s ) -> keyNew( %s )", applicationName, systemConfigRoot );
		keyDel( new->userRoot );
		ksDel ( new->userConfig );
		ksDel ( new->systemConfig );
		free( new );
		return;
	}

	retval = kdbGet ( ( KDB *) keysDBHandle, new->userConfig, new->userRoot, 0 );
	retval = kdbGet ( ( KDB *) keysDBHandle, new->systemConfig, new->systemRoot, 0 );

	strlcpy ( new->name, applicationName, NAME_MAX + 1 );
	addListEntryUnlocked ( keysList, new );
	unlockList( keysList );
}

void cleanupKeys ( const char *applicationName )
{
	struct appKey * entry;

	lockList( keysList, WRITE_LOCK );
	entry = findListEntryUnlocked ( keysList, ( char * ) applicationName );
	if ( entry )
	{
		kdbSet ( ( KDB *) keysDBHandle, entry->userConfig, 0, 0 );
		kdbSet ( ( KDB *) keysDBHandle, entry->systemConfig, 0, 0 );

		keyDel ( entry->userRoot );
		keyDel ( entry->systemRoot );

		ksDel ( entry->userConfig );
		ksDel ( entry->systemConfig );

		delListEntryUnlocked ( keysList, entry );
		putListEntry ( keysList, entry );
	}

	unlockList( keysList );
}

static char *__getCharKey ( KeySet *set, const char *key )
{
	Key * current;
	int found = 0;
	char keyName[ NAME_MAX + 1 ];
	char value[ BUFSIZ ];

	ksRewind ( set );
	while ( ( current = ksNext ( set ) ) )
	{
		keyGetBaseName ( current, keyName, sizeof ( keyName ) );
		if ( strcmp ( keyName, key ) == 0 )
		{
			found = 1;
			break;
		}
	}

	if ( found )
	{
		keyGetString ( current, value, sizeof ( value ) );
		if ( value[ 0 ] != 0 )
			return strdup ( value );
	}

	return NULL;
}

char *getCharKey ( const char *applicationName, const char *key, const char *defaultValue )
{
	char * result = NULL;
	struct appKey *app;

	app = findListEntry ( keysList, ( char * ) applicationName );
	if ( !app )
		return defaultValue ? strdup ( defaultValue ) : NULL;

	result = __getCharKey ( app->userConfig, key );
	if ( !result )
		result = __getCharKey ( app->systemConfig, key );

	putListEntry ( keysList, app );
	if ( result )
		return result;

	return defaultValue ? strdup ( defaultValue ) : NULL;
}

struct key *forEveryKey2 ( const char *applicationName, int mode )
{
	struct key *result = NULL;
	struct appKey *app;
	Key *current;
	char *ptr;
	char parentName[ PATH_MAX ];

	app = findListEntry ( keysList, ( char * ) applicationName );
	if ( !app )
		return NULL;

	if ( mode == USER_ROOT )
		current = ksNext ( app->userConfig );
	else
		current = ksNext ( app->systemConfig );

	if ( !current )
		goto out;

	result = malloc ( sizeof ( struct key ) );
	if ( !result )
		goto out;

	keyGetBaseName ( current, result->name, sizeof ( result->name ) );
	keyGetString ( current, result->value, sizeof ( result->value ) );
	keyGetParentName ( current, parentName, sizeof ( result->parentName ) );
	ptr = strrchr ( parentName, '/' );
	if ( ptr )
		strlcpy ( result->parentName, ptr+1, NAME_MAX + 1 );
	else
		strlcpy ( result->parentName, parentName, NAME_MAX + 1 );

	if ( keyIsDir ( current ) )
		result->type = LNC_CONFIG_DIR;
	else
		result->type = LNC_CONFIG_KEY;

out:
		putListEntry ( keysList, app );

	return result;
}


size_t getKeyCount ( const char *applicationName, int mode )
{
	struct appKey * app;
	size_t size;

	app = findListEntry ( keysList, ( char * ) applicationName );
	if ( !app )
		return -EXIT_FAILURE;

	if ( mode == USER_ROOT )
		size = ksGetSize ( app->userConfig );
	else
		size = ksGetSize ( app->systemConfig );

	putListEntry ( keysList, app );

	return size;
}

void rewindKeys ( const char *applicationName, int mode )
{
	struct appKey * app;

	app = findListEntry ( keysList, ( char * ) applicationName );
	if ( !app )
		return ;

	if ( mode == USER_ROOT )
		ksRewind ( app->userConfig );
	else
		ksRewind ( app->systemConfig );

	putListEntry ( keysList, app );
}

static int64_t __getNumKey ( KeySet *set, const char *key )
{
	Key * current;
	int found = 0;
	char keyName[ NAME_MAX + 1 ];
	char value[ BUFSIZ ];
	char *ptr;
	int64_t isLongLong;

	ksRewind ( set );

	while ( ( current = ksNext ( set ) ) )
	{
		keyGetBaseName ( current, keyName, sizeof ( keyName ) );
		if ( strcmp ( keyName, key ) == 0 )
		{
			found = 1;
			break;
		}

	}

	if ( found )
	{
		keyGetString ( current, value, sizeof ( value ) );
		if ( value[ 0 ] != '\0' )
		{
			isLongLong = strtoll ( value, &ptr, 10 );
			if ( isLongLong != LONG_LONG_MIN && isLongLong != LONG_LONG_MAX && ( *ptr == '\0' ) )
			{
				errno = 0;
				return isLongLong;
			}
		}
	}

	errno = ENOENT;
	return ( int64_t ) 0LL;
}

/*void __delKey( KeySet *set, const char *key ) {
	Key * current, *previous;
	char keyName[ 200 ];

	for ( previous = current = set->start; current; previous = current, current = current->next ) {
			keyGetBaseName( current, keyName, sizeof( keyName ) );
			if ( strcmp( keyName, key ) == 0 ) {
				if ( previous == set->start )
					set->start = current->next;
				else
					previous->next = current->next;
				kdbRemoveKey( current );
				keyDel( current );
				break;
			}
		}
}

void delKey( const char *applicationName, const char *key, const int flags ) {
	struct appKey * app;
	app = findListEntry( keysList, ( char * ) applicationName );
	if ( !app )
		return ;

	if ( flags & USER_ROOT )
		__delKey( &app->userConfig, key );

	if ( flags & SYSTEM_ROOT )
		__delKey( &app->systemConfig, key );

	putListEntry( keysList, app );
}*/

int setCharKey ( const char *applicationName, const char *key, const char *value, const int flags )
{
	Key * userNew, *systemNew;
	char userConfigRoot[ PATH_MAX ];
	char systemConfigRoot[ PATH_MAX ];
	char buffer[ BUFSIZ ];
	struct appKey *entry;
	struct passwd *user;

	entry = findListEntry ( keysList, ( char * ) applicationName );
	if ( !entry )
	{
		LNCdebug ( LNC_ERROR, 1, "setCharKey( %s, %s, %s, %d ) -> findListEntry()", applicationName, key, value, flags );
		printList ( keysList );
		return -EXIT_FAILURE;
	}

	if ( flags & USER_ROOT )
	{
		user = getSessionInfoByUID ( geteuid(), buffer );
		if ( !user )
		{
			LNCdebug ( LNC_ERROR, 1, "setCharKey( %s, %s, %s, %d ) -> getSessionInfoByUID( %d )", applicationName, key, value, flags, geteuid() );
			putListEntry ( keysList, entry );
			return -EXIT_FAILURE;
		}

		snprintf ( userConfigRoot, PATH_MAX, "user:%s/sw/%s/%s", user->pw_name, applicationName, key );
		userNew = keyNew ( userConfigRoot, KEY_END );
		keySetString ( userNew, value );
		ksAppendKey ( entry->userConfig, userNew );
	}

	if ( flags & SYSTEM_ROOT )
	{
		snprintf ( systemConfigRoot, PATH_MAX, "system/sw/%s/%s", applicationName, key );
		systemNew = keyNew ( systemConfigRoot, KEY_END );
		keySetString ( systemNew, value );
		ksAppendKey ( entry->systemConfig, systemNew );
	}

	putListEntry ( keysList, entry );

	return EXIT_SUCCESS;
}

int setNumKey ( const char *applicationName, const char *key, const int64_t value, const int flags )
{
	Key * userNew, *systemNew;
	char userConfigRoot[ PATH_MAX ];
	char systemConfigRoot[ PATH_MAX ];
	char buffer[ BUFSIZ ];
	struct appKey *entry;
	struct passwd *user;
	char tmp[ BUFSIZ ];

	sprintf ( tmp, "%lld", ( long long int ) value );
	entry = findListEntry ( keysList, ( char * ) applicationName );
	if ( !entry )
		return -EXIT_FAILURE;

	if ( flags & USER_ROOT )
	{
		user = getSessionInfoByUID ( geteuid(), buffer );
		if ( !user )
		{
			LNCdebug ( LNC_ERROR, 1, "setNumKey( %s, %s, %lld, %d ) -> getSessionInfoByUID( %d )", applicationName, key, value, flags, geteuid() );
			putListEntry ( keysList, entry );
			return -EXIT_FAILURE;
		}

		snprintf ( userConfigRoot, PATH_MAX, "user:%s/sw/%s/%s", user->pw_name, applicationName, key );
		userNew = keyNew ( userConfigRoot, KEY_END );
		keySetString ( userNew, tmp );
		ksAppendKey ( entry->userConfig, userNew );
	}

	if ( flags & SYSTEM_ROOT )
	{
		snprintf ( systemConfigRoot, PATH_MAX, "system/sw/%s/%s", applicationName, key );
		systemNew = keyNew ( systemConfigRoot, KEY_END );
		keySetString ( systemNew, tmp );
		ksAppendKey ( entry->systemConfig, systemNew );
	}

	putListEntry ( keysList, entry );
	return EXIT_SUCCESS;
}

int64_t getNumKey ( const char *applicationName, const char *key, const int64_t defaultValue )
{
	int64_t result;
	struct appKey *app;

	app = findListEntry ( keysList, ( char * ) applicationName );
	if ( !app )
		return defaultValue;

	result = __getNumKey ( app->userConfig, key );
	if ( errno != 0 )
		result = __getNumKey ( app->systemConfig, key );

	putListEntry ( keysList, app );
	if ( errno == 0 )
		return result;
	return defaultValue;
}

/*static int pollFileMonitor( struct monitor *file ) {
	struct stat buf;
	int retval, freq = 5;
	time_t prev_mtime = 0;

	LNCdebug( LNC_INFO, 0, "Starting pollFileMonitor()" );
	retval = stat( file->filename, &buf );
	if ( retval < 0 ) {
		LNCdebug( LNC_ERROR, 1, "pollFileMonitor(%s) -> stat(%s)", file->filename, file->filename );
		return -EXIT_FAILURE;
	}

	prev_mtime = buf.st_mtime;

	while ( 1 ) {
		if ( sleep( freq ) != 0 ) {
			LNCdebug( LNC_ERROR, 1, "pollFileMonitor( %s ) -> sleep( %d )" , file->filename, freq );
			return -EXIT_FAILURE;
		}
		retval = stat( file->filename, &buf );
		if ( retval < 0 ) {
			LNCdebug( LNC_ERROR, 1, "pollFileMonitor(%s) -> stat(%s)", file->filename, file->filename );
			return -EXIT_FAILURE;
		}

		if ( prev_mtime != buf.st_mtime ) {
			retval = file->func( file->arg, 0 );
			if ( retval < 0 ) {
				LNCdebug( LNC_ERROR, 1, "pollFileMonitor(%s) -> file->func(): failed to execute, returning", file->filename );
				return -EXIT_FAILURE;
			}
			prev_mtime = buf.st_mtime;
		}
	}

	return EXIT_SUCCESS;
}*/

defineList ( fileMonitorList, struct monitor );
struct fileMonitorList fileMonitorList;
pthread_t fileMonitorTID = 0UL;
pthread_mutex_t fileMonitorMutex = PTHREAD_MUTEX_INITIALIZER;
//pthread_cond_t fileMonitorCond = PTHREAD_COND_INITIALIZER;
pthread_once_t fileMonitorInit = PTHREAD_ONCE_INIT;
pthread_key_t printKey;
fd_set fileMonitorFDS;
int famFD;
int fileMonitorExiting = 0;
int fileMonitorInitialized = 0;
FAMConnection famConnection;

char eventStrings[ 10 ][ NAME_MAX + 1 ] = {
	"",
 	"FAMChanged",
 	"FAMDeleted",
 	"FAMStartExecuting",
 	"FAMStopExecuting",
 	"FAMCreated",
 	"FAMMoved",
 	"FAMAcknowledge",
 	"FAMExists",
 	"FAMEndExist"
};

const char *printFileMonitorEvent( int event, char *buffer, size_t size ) {
	snprintf( buffer, size, "%s", eventStrings[ event ] );

	return buffer;
}

void *fileMonitor ( void *data )
{
	FAMEvent famEvent;
	int retval;
	int error = -EXIT_FAILURE;
	fd_set tmpFDS;
	char *filename;
	struct monitor *file = NULL;
	struct timeval time;
	struct list *next;
	u_int32_t currentTime;

	( void ) data;

	LNCdebug ( LNC_INFO, 0, "Starting fileMonitor()" );

	fileMonitorTID = pthread_self();
/*	pthread_mutex_lock ( &fileMonitorMutex );
	pthread_cond_signal ( &fileMonitorCond );
	pthread_mutex_unlock ( &fileMonitorMutex );*/

	while ( 1 )
	{
		int i;

		time.tv_usec = 500000;
		time.tv_sec = 0;
		tmpFDS = fileMonitorFDS;

		retval = select ( famFD + 1, &tmpFDS, NULL, NULL, &time );

		if ( fileMonitorExiting )
		{

			error = EXIT_SUCCESS;
			goto out;
		}

		if ( time.tv_sec == 0 && time.tv_usec == 0 )
			continue;

		if ( retval < 0 )
		{
			if ( errno == EINTR )
				continue;
			LNCdebug ( LNC_ERROR, 1, "famFileMonitor() -> select()" );
			goto out;
		}

		usleep ( 500000 );
		for ( i = 0; FAMPending ( &famConnection ); i++ )
		{

			retval = FAMNextEvent ( &famConnection, &famEvent );
			if ( retval < 0 )
			{
				LNCdebug ( LNC_ERROR, 1, "famFileMonitor() -> FAMNextEvent(), retval = \"%d\"", retval );
				goto out;
			}

			filename = ( char * ) famEvent.userdata;

			currentTime = getCurrentTime();
retry:
			lockList( &fileMonitorList, READ_LOCK );
			forEachListEntry ( &fileMonitorList, file )
			{
				if ( strcmp ( filename, file->filename ) != 0 )
					continue;

				if ( fileMonitorExiting ) {
					unlockList ( &fileMonitorList );
					error = EXIT_SUCCESS;
					goto out;
				}

				if ( file->callback && file->timeStamp != currentTime ) {
					next = file->list.next;
					getListEntry( &fileMonitorList, file );
					atomic_inc( &next->count );
					unlockList ( &fileMonitorList );

					file->callback ( file->filename, famEvent.filename,  famEvent.code );

					lockList( &fileMonitorList, READ_LOCK );
					file->timeStamp = currentTime;
					if ( file->list.next == NULL || next->next == NULL ) {
						putListEntry( &fileMonitorList, file );
						atomic_dec( &next->count );
						unlockList ( &fileMonitorList );
						goto retry;
					}
					putListEntry( &fileMonitorList, file );
					atomic_dec( &next->count );
				}

			}

			unlockList ( &fileMonitorList );
		}
	}

	error = EXIT_SUCCESS;

out:
//	pthread_mutex_lock ( &fileMonitorMutex );
//	fileMonitorTID = 0;
//	pthread_mutex_unlock ( &fileMonitorMutex );
	return int_to_void_cast error;
}

int fileMonitorCompare ( struct monitor *new, struct monitor *entry )
{
	int cmp;

	cmp = strcmp ( new->filename, entry->filename );
	if ( cmp == 0 )
		return -1;

	return cmp;
}

/*void fileMonitorOnInsert ( struct monitor *file )
{
	int retval;
	struct stat stbuf;

	retval = stat ( file->filename, &stbuf );
	if ( retval == 0 && ( stbuf.st_mode & S_IFMT ) == S_IFDIR )
	{
		retval = FAMMonitorDirectory ( &famConnection, file->filename, &file->request, file->filename );
		if ( retval < 0 )
			LNCdebug ( LNC_ERROR, 1, "fileMonitorOnInsert( %s ) -> FAMMonitorDirectory()", file->filename );
	}
	else
	{
		retval = FAMMonitorFile ( &famConnection, file->filename, &file->request, file->filename );
		if ( retval < 0 )
			LNCdebug ( LNC_ERROR, 1, "fileMonitorOnInsert %s ) -> FAMMonitorFile()", file->filename );
	}
}*/

void fileMonitorFree ( struct monitor *entry )
{
	free ( entry->filename );
	free ( entry );
}

void initFileMonitor ( void )
{
	int retval;

	initList ( &fileMonitorList, fileMonitorCompare, NULL, fileMonitorFree, NULL, /*fileMonitorOnInsert*/NULL, NULL, NULL );
	FD_ZERO ( &fileMonitorFDS );
	retval = FAMOpen ( &famConnection );
	if ( retval < 0 )
	{
		errno = FAMErrno;
		LNCdebug ( LNC_ERROR, 1, "initFileMonitor() -> FAMopen()" );
		return ;
	}

	famFD = FAMCONNECTION_GETFD ( &famConnection );
	FD_SET ( famFD, &fileMonitorFDS );
	fileMonitorInitialized = 1;
}

int addFileToMonitorList ( const char *filename, monitorFunction *callback )
{
	int retval;
	struct monitor * file;
	struct stat stbuf;

	LNCdebug ( LNC_INFO, 0, "addFileToMonitorList(): adding %s to list", filename );

	if ( !filename || !callback ) {
		errno = EINVAL;
		LNCdebug( LNC_ERROR, 1, "addFileToMonitorList( %s )", filename );
	}

	lockLNCConfig( READ_LOCK );
	if ( networkConfig.EnableDebugMode ) {
		unlockLNCConfig();
		return EXIT_SUCCESS;
	}
	unlockLNCConfig();

	pthread_once ( &fileMonitorInit, initFileMonitor );

	pthread_mutex_lock ( &fileMonitorMutex );
	if ( !fileMonitorInitialized )
	{
		LNCdebug ( LNC_ERROR, 0, "addFileToMonitorList( %s ): file monitor failed during initialization", filename );
		errno = ECONNREFUSED;
		pthread_mutex_unlock ( &fileMonitorMutex );
		return -EXIT_FAILURE;
	}

	file = malloc ( sizeof ( struct monitor ) );
	if ( !file )
	{
		LNCdebug ( LNC_ERROR, 1, "addFileToMonitorList( %s ) -> malloc()", filename );
		pthread_mutex_unlock ( &fileMonitorMutex );
		return -EXIT_FAILURE;
	}

	file->filename = strdup ( ( char * ) filename );
	file->callback = callback;

	retval = stat ( file->filename, &stbuf );
	if ( retval == 0 && ( stbuf.st_mode & S_IFMT ) == S_IFDIR )
	{
		retval = FAMMonitorDirectory ( &famConnection, file->filename, &file->request, file->filename );
		if ( retval < 0 )
			LNCdebug ( LNC_ERROR, 1, "addFileToMonitorList( %s ) -> FAMMonitorDirectory()", file->filename );
	}
	else
	{
		retval = FAMMonitorFile ( &famConnection, file->filename, &file->request, file->filename );
		if ( retval < 0 )
			LNCdebug ( LNC_ERROR, 1, "addFileToMonitorList( %s ) -> FAMMonitorFile()", file->filename );
	}

	if ( retval < 0 ) {
		free( file );
		return -EXIT_FAILURE;
	}

	addListEntry ( &fileMonitorList, file );

	if ( fileMonitorTID )
	{
		pthread_mutex_unlock ( &fileMonitorMutex );
		return EXIT_SUCCESS;
	}

	if ( !createThread ( fileMonitor, NULL, LOOP_MODE ) )
	{
		LNCdebug ( LNC_ERROR, 1, "addFileToMonitorList( %s ) -> createThread( fileMonitor )", filename );
		pthread_mutex_unlock ( &fileMonitorMutex );
		return -EXIT_FAILURE;
	}

//	pthread_cond_wait ( &fileMonitorCond, &fileMonitorMutex );
	pthread_mutex_unlock ( &fileMonitorMutex );

	return EXIT_SUCCESS;
}

void delFileFromMonitorList( const char *filename, monitorFunction *callback ) {

	int cnt;
	struct monitor *entry;
	struct monitor *file;

	LNCdebug ( LNC_INFO, 0, "delFileFromMonitorList(): deleting %s from list", filename );

	if ( !filename || !callback ) {
		errno = EINVAL;
		LNCdebug( LNC_ERROR, 1, "delFileFromMonitorList( %s )", filename );
	}

	file = NULL;
	cnt = 0;
	lockList ( &fileMonitorList, WRITE_LOCK );
	forEachListEntry ( &fileMonitorList, entry )
	{
		if ( strcmp( filename, entry->filename ) != 0 )
			continue;

		++cnt;
		if ( callback == entry->callback )
			file = entry;
	}

	if ( file ) {
		if ( cnt == 1 ) {
			LNCdebug( LNC_INFO, 0, "delFilefromMonitorList( %s ): file only monitored once, doing FAMCancelMonitor() ", filename );
			FAMCancelMonitor ( &famConnection, &file->request );
		}
		delListEntryUnlocked ( &fileMonitorList, file );
	}

	unlockList ( &fileMonitorList );
}

void stopFileMonitor ( void )
{
	struct monitor * file;
	int retval;
	void *result;

	LNCdebug ( LNC_INFO, 0, "stopFileMonitor(): Stopping file monitor" );
	pthread_once ( &fileMonitorInit, initFileMonitor );

	pthread_mutex_lock ( &fileMonitorMutex );

	fileMonitorExiting = 1;
	if ( fileMonitorTID )
	{
		retval = pthread_join ( fileMonitorTID, &result );
		if ( retval < 0 )
			LNCdebug( LNC_ERROR, 1, "stopFileMonitor() -> pthread_join( %lu )", fileMonitorTID );
		else
			LNCdebug( LNC_INFO, 0, "stopFileMonitor() -> pthread_join( %lu ): thread exited with exit status %d", fileMonitorTID, void_to_int_cast result );
		fileMonitorTID = 0;
	}

	lockList ( &fileMonitorList, WRITE_LOCK );
	forEachListEntry ( &fileMonitorList, file )
	{
		FAMCancelMonitor ( &famConnection, &file->request );
		delListEntryUnlocked ( &fileMonitorList, file );
	}

	FAMClose ( &famConnection );
	unlockList ( &fileMonitorList );
	pthread_mutex_unlock ( &fileMonitorMutex );
}
