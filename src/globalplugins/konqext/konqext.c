/***************************************************************************
*   Copyright (C) 2003 by Usarin Heininga                                 *
*   usarin@heininga.nl                                                    *
*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License as published by  *
*   the Free Software Foundation; either version 2 of the License, or     *
*   (at your option) any later version.                                   *
***************************************************************************/

#include "config.h"
#include "networkclient.h"
#include "list.h"
#include "nconfig.h"
#include "plugin.h"
#include "pluginhelper.h"
#include "user.h"
#include "nconfig.h"
#include "fusefs.h"
#include "daemon.h"
#include "display.h"

#include "konq_dcop.h"

#include <stdlib.h>

#define LOGINAS_ACTION				1
#define CREATESHORTCUT_ACTION			2
#define MAPNETWORKDRIVE_ACTION			4
#define HOST_ICON				8
#define SHARE_ICON				16

int kmapdriveNotInstalled = 0;
int konqLoginAsNotInstalled = 0;
int krdcNotInstalled = 0;
int sshNotInstalled = 0;
int shareIconLength[ 2 ] = { 0, 0 };
int hostIconLength[ 2 ] = { 0, 0 };


extern int getActiveWindowID ( void );
extern NetworkPlugin *getProtocolPluginByURL ( const char *URL, int resolveShortcuts );
extern int isShortcut ( const char *path );

static int doesExecutableExist ( const char *executable );
static int isDirectoryFile ( const char *URL );
static char *createIconBuffer ( unsigned int flags ) ;
static int isDirectoryFile ( const char *path );
static int statIcon ( int depth, const char *URL, struct stat *buf );
static int readIcon ( int depth, const char *URL, void *buf, size_t size, off_t offset );
static void *errorRoutine ( void *arg );
static int konqExtStatHook ( const char *URL, struct stat *buf, int *replace, int runMode, int result );
static NETWORK_FILE *konqExtOpenHook ( const char *URL, int flags, mode_t mode, int *replace, int runMode, NETWORK_FILE *result );
static int konqExtCloseHook ( NETWORK_FILE *file, int *replace, int runMode, int result );
static ssize_t konqExtReadHook ( NETWORK_FILE *file, void *buf, size_t count, int *replace, int runMode, ssize_t result );
static int konqExtInit ( void );

static int doesExecutableExist ( const char *executable )
{
	char *output;
	char exec[ PATH_MAX ];
	int exitStatus;
	int retval;

	snprintf ( exec, PATH_MAX, "bash -c \"type %s\"", executable );
	output = execute ( exec, STDOUT_FILENO | STDERR_FILENO, environ, &exitStatus );
	if ( !output )
	{
		LNCdebug ( LNC_ERROR, 0, "doesExecutableExist( %s ) -> execute( %s ): No output available", executable, exec );
		return -EXIT_FAILURE;
	}

	retval = 1;
	if ( strstr ( output, "not found" ) )
		retval =  0;

	free( output );

	return retval;
}

static int isDirectoryFile ( const char *URL )
{
	char * ptr;

	ptr = strrchr ( URL, '/' );
	if ( !ptr )
		return 0;
	else if ( strcasecmp ( ptr + 1, ".directory" ) != 0 && strcasecmp ( ptr + 1, ".hidden" ) != 0 )
		return 0;

	return 1;
}

static char *printTimeStamp( char *iconBuffer )
{
	char *ptr;
	char dateBuffer[ 20 ];
	int len;
	struct timeval tv;
	struct tm buf;
	struct tm *result;

	gettimeofday( &tv, NULL );
	result = localtime_r( &tv.tv_sec, &buf );
	if ( !result )
		return iconBuffer;

	snprintf( dateBuffer, 20, "%d,%d,%d,%d,%d,%d", result->tm_year + 1900, result->tm_mon + 1, result->tm_mday, result->tm_hour, result->tm_min, result->tm_sec );
	len = strlen( dateBuffer );

	ptr = strstr( iconBuffer, "Timestamp=" );
	if ( !ptr )
		return iconBuffer;

	strncpy( ptr+10, dateBuffer, len );

	ptr = strstr( ptr+10, "Timestamp=" );
	if ( !ptr )
		return iconBuffer;

	strncpy( ptr+10, dateBuffer, len );

	return iconBuffer;
}

static char *removeLoginAsAction ( char *iconBuffer )
{
	char * tmpBuffer = strdup ( iconBuffer );
	char *begin;
	char *end;

	begin = strstr ( tmpBuffer, "[Desktop Action loginas]" );
	if ( begin )
	{
		end = strstr ( begin + 1, "[Desktop Action" );
		*begin = '\0';
		snprintf ( iconBuffer, strlen ( iconBuffer ), "%s%s", tmpBuffer, end ? end : "" );
	}

	strcpy ( tmpBuffer, iconBuffer );
	begin = strstr ( tmpBuffer, "loginas" );
	if ( begin )
	{
		end = begin + 7;
		if ( *end == ';' )
			end += 1;
		*begin = '\0';
		snprintf ( iconBuffer, strlen ( iconBuffer ), "%s%s", tmpBuffer, end ? end : "" );
	}

	free ( tmpBuffer );

	return iconBuffer;
}

static char *removeCreateShortcutAction ( char *iconBuffer )
{
	char * tmpBuffer = strdup ( iconBuffer );
	char *begin;
	char *end;

	begin = strstr ( tmpBuffer, "[Desktop Action createshortcut]" );
	if ( begin )
	{
		end = strstr ( begin + 1, "[Desktop Action" );
		*begin = '\0';
		snprintf ( iconBuffer, strlen ( iconBuffer ), "%s%s", tmpBuffer, end ? end : "" );
	}

	strcpy ( tmpBuffer, iconBuffer );
	begin = strstr ( tmpBuffer, "createshortcut" );
	if ( begin )
	{
		end = begin + 14;
		if ( *end == ';' )
			end += 1;
		*begin = '\0';
		snprintf ( iconBuffer, strlen ( iconBuffer ), "%s%s", tmpBuffer, end ? end : "" );
	}

	free ( tmpBuffer );

	return iconBuffer;
}

static char *removeMapNetworkDriveAction ( char *iconBuffer )
{
	char * tmpBuffer = strdup ( iconBuffer );
	char *begin;
	char *end;

	begin = strstr ( tmpBuffer, "[Desktop Action mapnetworkdrive]" );
	if ( begin )
	{
		end = strstr ( begin + 1, "[Desktop Action" );
		*begin = '\0';
		snprintf ( iconBuffer, strlen ( iconBuffer ), "%s%s", tmpBuffer, end ? end : "" );
	}

	strcpy ( tmpBuffer, iconBuffer );
	begin = strstr ( tmpBuffer, "mapnetworkdrive" );
	if ( begin )
	{
		end = begin + 15;
		if ( *end == ';' )
			end += 1;
		*begin = '\0';
		snprintf ( iconBuffer, strlen ( iconBuffer ), "%s%s", tmpBuffer, end ? end : "" );
	}

	free ( tmpBuffer );

	return iconBuffer;
}

static char *removeRemoteDesktopConnectionAction ( char *iconBuffer )
{
	char * tmpBuffer = strdup ( iconBuffer );
	char *begin;
	char *end;

	begin = strstr ( tmpBuffer, "[Desktop Action remotedesktopconnection]" );
	if ( begin )
	{
		end = strstr ( begin + 1, "[Desktop Action" );
		*begin = '\0';
		snprintf ( iconBuffer, strlen ( iconBuffer ), "%s%s", tmpBuffer, end ? end : "" );
	}

	strcpy ( tmpBuffer, iconBuffer );
	begin = strstr ( tmpBuffer, "remotedesktopconnection" );
	if ( begin )
	{
		end = begin + 23;
		if ( *end == ';' )
			end += 1;
		*begin = '\0';
		snprintf ( iconBuffer, strlen ( iconBuffer ), "%s%s", tmpBuffer, end ? end : "" );
	}

	free ( tmpBuffer );

	return iconBuffer;
}

static char *createIconBuffer ( unsigned int flags )
{
	char iconPath[ PATH_MAX ];
	char *iconBuffer;
	int retval;
	int fd;
	struct stat stbuf;

	snprintf ( iconPath, PATH_MAX, "%s/%s", APP_DATA_DIR, ( flags & SHARE_ICON ) ? "share.directory" : "host.directory" );
	retval = stat ( iconPath, &stbuf );
	if ( retval < 0 )
	{
		LNCdebug ( LNC_ERROR, 1, "createIconBuffer( %u ) -> stat( %s )", flags, iconPath );
		return NULL;
	}
	else if ( stbuf.st_size <= 0 )
	{
		LNCdebug ( LNC_ERROR, 0, "createIconBuffer( %u ) -> stat( %s ): file to small", flags, iconPath );
		return NULL;
	}

	fd = open ( iconPath, O_RDONLY );
	if ( fd < 0 )
	{
		LNCdebug ( LNC_ERROR, 1, "createIconBuffer( %u ) -> open( %s, O_RDONLY )", flags, iconPath );
		return NULL;
	}

	iconBuffer = malloc ( sizeof ( char ) * ( stbuf.st_size + 1 ) );
	if ( !iconBuffer )
	{
		LNCdebug ( LNC_ERROR, 1, "createIconBuffer( %u ) -> malloc()", flags );
		return NULL;
	}

	retval = read ( fd, iconBuffer, stbuf.st_size );
	if ( retval < 0 )
	{
		LNCdebug ( LNC_ERROR, 1, "createIconBuffer( %u ) -> pread( %d, iconBuffer, %d )", flags, fd, stbuf.st_size + 1 );
		free ( iconBuffer );
		close ( fd );
		return NULL;
	}
	close ( fd );
	iconBuffer[ stbuf.st_size ] = '\0';

	if ( ! ( flags & LOGINAS_ACTION ) || konqLoginAsNotInstalled )
		iconBuffer = removeLoginAsAction ( iconBuffer );

	if ( ! ( flags & CREATESHORTCUT_ACTION ) )
		iconBuffer = removeCreateShortcutAction ( iconBuffer );

	if ( ! ( flags & MAPNETWORKDRIVE_ACTION ) || kmapdriveNotInstalled )
		iconBuffer = removeMapNetworkDriveAction ( iconBuffer );

	if ( krdcNotInstalled )
		iconBuffer = removeRemoteDesktopConnectionAction ( iconBuffer );

	return iconBuffer;
}

static void calculateIconLenghts ( void )
{
	char iconPath[ PATH_MAX ];
	char buffer[ BUFSIZ ];
	char *ptr;
	int fd;
	int retval;

	snprintf ( iconPath, PATH_MAX, "%s/%s", APP_DATA_DIR, "share.directory" );
	fd = open ( iconPath, O_RDONLY );
	if ( fd < 0 )
	{
		LNCdebug ( LNC_ERROR, 1, "calculateIconLenghts() -> open( %s, O_RDONLY )", iconPath );
		return ;
	}

	retval = pread ( fd, buffer, BUFSIZ, 0 );
	if ( retval < 0 )
	{
		LNCdebug ( LNC_ERROR, 1, "calculateIconLenghts() -> pread( %d, buffer, %d )", fd, BUFSIZ );
		return ;
	}

	shareIconLength[ 0 ] = retval;
	ptr = strstr ( buffer, "Actions" );
	if ( ptr )
	{
		*ptr = '\0';
		shareIconLength[ 1 ] = strlen ( buffer );
	}
	else
		shareIconLength[ 1 ] = retval;

	snprintf ( iconPath, PATH_MAX, "%s/%s", APP_DATA_DIR, "host.directory" );
	fd = open ( iconPath, O_RDONLY );
	if ( fd < 0 )
	{
		LNCdebug ( LNC_ERROR, 1, "calculateIconLenghts() -> open( %s, O_RDONLY )", iconPath );
		return ;
	}

	retval = pread ( fd, buffer, BUFSIZ, 0 );
	if ( retval < 0 )
	{
		LNCdebug ( LNC_ERROR, 1, "calculateIconLenghts() -> pread( %d, buffer, %d )", fd, BUFSIZ );
		close ( fd );
		return ;
	}
	close ( fd );

	hostIconLength[ 0 ] = retval;
	ptr = strstr ( buffer, "Actions" );
	if ( ptr )
	{
		*ptr = '\0';
		hostIconLength[ 1 ] = strlen ( buffer );
	}
	else
		hostIconLength[ 1 ] = retval;
}

static int statIcon ( int depth, const char *URL, struct stat *buf )
{
	char *iconBuffer;
	char icon[ NAME_MAX + 1 ];
	char iconPath[ PATH_MAX ];
	int retval;
	int flags;
	NetworkPlugin *plugin;

	if ( depth == 6 )
		strcpy ( icon, "share.directory" );
	else if ( depth == 5 )
		strcpy ( icon, "host.directory" );
	else if ( depth == 4 )
		strcpy ( icon, "domain.directory" );
	else if ( depth == 3 )
	{
		NetworkPlugin * plugin;

		plugin = getProtocolPluginByURL ( URL, 0 );
		if ( plugin )
		{
			strlcpy ( icon, plugin->protocol, NAME_MAX + 1 );
			strlcpy ( icon + plugin->protocolLength, ".directory", NAME_MAX + 1 - plugin->protocolLength );
			putListEntry ( networkPluginList, plugin );
		}
		else
			strcpy ( icon, "unknown.directory" );
	}
	else if ( depth == 2 )
		strcpy ( icon, "network.directory" );

	snprintf ( iconPath, PATH_MAX, "%s/%s", APP_DATA_DIR, icon );
	retval = lstat ( iconPath, buf );
	if ( retval < 0 )
	{
		LNCdebug ( LNC_ERROR, 1, "statIcon( %d, %s, buf ) -> lstat( %s, buf )", depth, URL, iconPath );
		return retval;
	}

	if ( depth == 6 || depth == 5 )
	{
		plugin = getProtocolPluginByURL ( URL, 1 );
		if ( plugin )
		{
			flags = 0;
			if ( depth == 6 )
				flags |= SHARE_ICON;
			else
				flags |= HOST_ICON;


			if ( depth == 6 || ( depth == 5 && !plugin->getShares ) )
			{
				flags |= MAPNETWORKDRIVE_ACTION;

				if ( !isShortcut ( URL ) )
					flags |= CREATESHORTCUT_ACTION;
			}

			if ( plugin->supportsAuthentication )
				flags |= LOGINAS_ACTION;


			iconBuffer = createIconBuffer ( flags );
			buf->st_size = strlen ( iconBuffer );
			free ( iconBuffer );

			putListEntry ( networkPluginList, plugin );
		}
	}

	return EXIT_SUCCESS;
}

static int readIcon ( int depth, const char *URL, void *buf, size_t size, off_t offset )
{
	char * iconBuffer;
	char icon[ NAME_MAX + 1 ];
	char iconPath[ PATH_MAX ];
	int fd;
	int retval;
	unsigned int flags;
	unsigned int len;
	NetworkPlugin *plugin;

	if ( depth == 6 )
		strcpy ( icon, "share.directory" );
	else if ( depth == 5 )
		strcpy ( icon, "host.directory" );
	else if ( depth == 4 )
		strcpy ( icon, "domain.directory" );
	else if ( depth == 3 )
	{
		plugin = getProtocolPluginByURL ( URL, 0 );
		if ( plugin )
		{
			strlcpy ( icon, plugin->protocol, NAME_MAX + 1 );
			strlcpy ( icon + plugin->protocolLength, ".directory", NAME_MAX + 1 - plugin->protocolLength );
			putListEntry ( networkPluginList, plugin );
		}
		else
			strcpy ( icon, "unknown.directory" );
	}
	else if ( depth == 2 )
		strcpy ( icon, "network.directory" );

	snprintf ( iconPath, PATH_MAX, "%s/%s", APP_DATA_DIR, icon );
	fd = open ( iconPath, O_RDONLY );
	if ( fd < 0 )
	{
		LNCdebug ( LNC_ERROR, 1, "readIcon( %d, %s, buf , %d, %d) -> open( %s, O_RDONLY )", depth, URL, size, offset, iconPath );
		return fd;
	}

	retval = pread ( fd, buf, size, offset );
	close ( fd );
	if ( retval > 0 && ( depth == 6 || depth == 5 ) )
	{
		plugin = getProtocolPluginByURL ( URL, 1 );
		if ( plugin )
		{
			flags = 0;
			if ( depth == 6 )
				flags |= SHARE_ICON;
			else
				flags |= HOST_ICON;

			if ( depth == 6 || ( depth == 5 && !plugin->getShares ) )
			{
				flags |= MAPNETWORKDRIVE_ACTION;

				if ( !isShortcut ( URL ) )
					flags |= CREATESHORTCUT_ACTION;
			}

			if ( plugin->supportsAuthentication )
				flags |= LOGINAS_ACTION;


			iconBuffer = createIconBuffer ( flags );
			snprintf ( buf, size, "%s", iconBuffer );
			len = strlen ( iconBuffer );
			free ( iconBuffer );
			retval = len > size ? size : len;

			putListEntry ( networkPluginList, plugin );
		}
	}

	if ( retval > 0 && depth == 2 ) {
		printTimeStamp( buf );
	}

	return retval;
}

static void *errorRoutine ( void *arg )
{
	char * display;
	char *Xauthority;
	char buffer[ BUFSIZ ];
	struct syscallContext *context;
	struct passwd *user;

	context = getSyscallContext();
	if ( !context )
	{
		LNCdebug ( LNC_ERROR, 1, "errorRoutine( %s ) -> getSyscallContext()", ( char * ) arg );
		return ( void * ) - EXIT_FAILURE;
	}

	if ( !context->application )
	{
		LNCdebug ( LNC_INFO, 0, "errorRoutine(): No context for this system call" );
		return ( void * ) - EXIT_FAILURE;
	}

	if ( strcmp ( context->application->name, "kio_file" ) != 0 )
	{
		LNCdebug ( LNC_INFO, 0, "errorRoutine(): Doesn't link to the proper library" );
		return ( void * ) - EXIT_FAILURE;
	}

	user = getSessionInfoByUID ( context->UID, buffer );
	if ( !user )
	{
		LNCdebug ( LNC_ERROR, 1, "errorRoutine( %s ) -> getSessionInfoByUID( %d )", ( char * ) arg, context->UID );
		return ( void * ) - EXIT_FAILURE;
	}

	display = getDisplayDevice ( context->PID, user );
	if ( display )
	{
		setenv ( "DISPLAY", display, 1 );
		Xauthority = getXauthority ( context->PID, display );
		if ( Xauthority )
		{
			setenv ( "XAUTHORITY", Xauthority, 1 );
			free ( Xauthority );
		}
		free ( display );
	}
	else
	{
		LNCdebug ( LNC_ERROR, 1, "errorRoutine( %s ) -> getDisplayDevice()", ( char * ) arg );
		return ( void * ) - EXIT_FAILURE;
	}

	showErrorMessage ( "Error", "Konqueror", arg, 15, getActiveWindowID() );

	return ( void * ) EXIT_SUCCESS;
}

static int konqExtStatHook ( const char *URL, struct stat *buf, int *replace, int runMode, int result )
{
	int depth;
	int retval;
	char *ptr;
	char cpy[ PATH_MAX ];
	Service *found;

	( void ) result;

	if ( runMode == AFTER_SYSCALL )
		return EXIT_SUCCESS;

	depth = getDepth ( URL );
	if ( depth < 2 || depth > 6 )
		return EXIT_SUCCESS;

	if ( !isDirectoryFile ( URL ) )
		return EXIT_SUCCESS;

	if ( depth == 2 )
	{
		*replace = 1;
		retval = statIcon ( depth, URL, buf );
		return retval < 0 ? -errno : retval;
	}

	snprintf ( cpy, PATH_MAX, "%s", URL );
	ptr = strrchr ( cpy, '/' );
	*ptr ='\0';
	found = findListEntry ( browselist, cpy );
	if ( !found )
		return EXIT_SUCCESS;

	if ( found->share )
		depth = 6;
	else if ( found->host )
		depth = 5;
	else if ( found->domain )
		depth = 4;
	else if ( found->network )
		depth = 3;
	else
		depth = 2;

	putListEntry ( browselist, found );

	*replace = 1;
	retval = statIcon ( depth, URL, buf );
	return retval < 0 ? -errno : retval;
}

static NETWORK_FILE *konqExtOpenHook ( const char *URL, int flags, mode_t mode, int *replace, int runMode, NETWORK_FILE *result )
{
	int depth;
	char *ptr;
	char cpy[ PATH_MAX ];
	Service *found;

	( void ) result;
	( void ) flags;
	( void ) mode;

	if ( runMode == AFTER_SYSCALL )
		return NULL;

	depth = getDepth ( URL );
	if ( depth < 2 || depth > 6 )
		return NULL;

	if ( !isDirectoryFile ( URL ) )
		return NULL;

	if ( depth == 2 )
	{
		*replace = 1;
		result = malloc ( sizeof ( NETWORK_FILE ) );
		strlcpy ( result->URL, URL, PATH_MAX );
		result->plugin = NULL;
		result->depth = depth;
		result->privateData = int_to_void_cast INT_MAX;
		return result;
	}

	snprintf ( cpy, PATH_MAX, "%s", URL );
	ptr = strrchr ( cpy, '/' );
	*ptr ='\0';
	found = findListEntry ( browselist, cpy );
	if ( !found )
		return EXIT_SUCCESS;

	if ( found->share )
		depth = 6;
	else if ( found->host )
		depth = 5;
	else if ( found->domain )
		depth = 4;
	else if ( found->network )
		depth = 3;
	else
		depth = 2;

	putListEntry ( browselist, found );

	*replace = 1;
	result = malloc ( sizeof ( NETWORK_FILE ) );
	strlcpy ( result->URL, URL, PATH_MAX );
	result->plugin = NULL;
	result->depth = depth;
	result->privateData = int_to_void_cast INT_MAX;

	return result;
}

static int konqExtCloseHook ( NETWORK_FILE *file, int *replace, int runMode, int result )
{
	( void ) result;

	if ( runMode == AFTER_SYSCALL )
		return EXIT_SUCCESS;

	if ( file->plugin )
		return EXIT_SUCCESS;

	if ( ( void_to_int_cast file->privateData ) != INT_MAX )
		return EXIT_SUCCESS;

	if ( file->depth >= 1 )
	{
		*replace = 1;
		free ( file );
		return EXIT_SUCCESS;
	}

	return EXIT_SUCCESS;
}

static ssize_t konqExtReadHook ( NETWORK_FILE *file, void *buf, size_t count, int *replace, int runMode, ssize_t result )
{
	int retval;

	( void ) result;

	if ( runMode == AFTER_SYSCALL )
		return EXIT_SUCCESS;

	if ( file->plugin )
		return EXIT_SUCCESS;

	if ( ( void_to_int_cast file->privateData ) != INT_MAX )
		return EXIT_SUCCESS;

	if ( file->depth >= 1 )
	{
		*replace = 1;
		retval = readIcon ( file->depth, file->URL, buf, count, 0 );
		return ( retval < 0 ) ? -errno : retval;
	}

	return EXIT_SUCCESS;
}

static int konqExtInit ( void )
{
	int found;

	calculateIconLenghts();

	found = doesExecutableExist ( "ssh" );
	if ( !found )
		sshNotInstalled = 1;

	found = doesExecutableExist ( "konqLoginAs" );
	if ( !found )
		konqLoginAsNotInstalled = 1;

	found = doesExecutableExist ( "kmapdrive" );
	if ( !found )
		kmapdriveNotInstalled = 1;

	found = doesExecutableExist ( "krdc" );
	if ( !found )
		krdcNotInstalled = 1;

	registerSyscallHook ( konqExtStatHook, LNC_STAT, PRE_SYSCALL );
	registerSyscallHook ( konqExtOpenHook, LNC_OPEN, PRE_SYSCALL );
	registerSyscallHook ( konqExtCloseHook, LNC_CLOSE, PRE_SYSCALL );
	registerSyscallHook ( konqExtReadHook, LNC_READ, PRE_SYSCALL );

	return EXIT_SUCCESS;
}

static void konqExtCleanUp( void )
{
	unregisterSyscallHook ( konqExtStatHook );
	unregisterSyscallHook ( konqExtOpenHook );
	unregisterSyscallHook ( konqExtCloseHook );
	unregisterSyscallHook ( konqExtReadHook );
}

static const char ErrorType[] = "Konqueror Extensions";
static const char ErrorDescription[] = "This plugin allows you to have KDE/QT styled error dialogs, when access is denied and provides a \"Login as\" menu entry";
static const char *ErrorDependencies[] = { "lib_global_plugin_Xext.so", NULL };

static GlobalPlugin error =
{
	.type = ErrorType,
	.description = ErrorDescription,
	.dependencies = ErrorDependencies,
	.init = konqExtInit,
 	.cleanUp = konqExtCleanUp,
	.pluginRoutine = errorRoutine,
};

void *giveGlobalPluginInfo ( void )
{
	return & error;
}
