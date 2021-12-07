/***************************************************************************
*   Copyright (C) 2003 by Usarin Heininga                                 *
*   usarin@heininga.nl                                                    *
*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License as published by  *
*   the Free Software Foundation; either version 2 of the License, or     *
*   (at your option) any later version.                                   *
***************************************************************************/

#include <linux/limits.h>
#include <string.h>
#include <malloc.h>
#include <limits.h>
#include <dirent.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/wait.h>
#include <ctype.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/kd.h>
#include <linux/vt.h>

#include "display.h"
#include "util.h"
#include "nconfig.h"
#include "plugin.h"

static int openConsole ( const char *filename );

/*char *getKdeinitProcessName( pid_t pid ) {
	char path[ 25 ];
	char *ptr;
	char *cmdline;
	char *realname;
	char name[ BUFSIZ ];
	int retval;

	sprintf( path, "/proc/%d/cmdline", pid );
	retval = copyFileToBuffer( &cmdline, path );
	if ( retval < 0 ) {
		LNCdebug( LNC_ERROR, 1, "getKdeinitProcessName( %d ) -> copyFileToBuffer( %s )", pid, path );
		return NULL;
	}

	name[ 0 ] = '\0';
	sscanf( cmdline, "kdeinit: %s", name );
	if ( name[ 0 ] == '\0' ) {
		free( cmdline );
		return strdup( "kdeinit" );
	}

	if ( strcmp( name, "kio_file" ) != 0 ) {
		free( cmdline );
		return strdup( "kdeinit" );
	}

	realname = strrchr( cmdline, '/' );
	if ( !realname ) {
		free( cmdline );
		return strdup( name );
	}

	++realname;
	ptr = strstr( realname, ".slave-socket" );
	if ( !ptr ) {
		free( cmdline );
		return strdup( name );
	}

	*( ptr - 6 ) = '\0';
	ptr = strdup( realname );
	free( cmdline );
	return ptr;
}*/

char *getCommandLineArguments ( pid_t pid )
{
	char * string;
	char path[ 25 ];
	char *ptr;
	char *CMDLine;
	int len;

	sprintf ( path, "/proc/%d/cmdline", pid );
	len = copyFileToBuffer ( &string, path );
	if ( len < 0 )
	{
		LNCdebug ( LNC_ERROR, 1, "getCommandLineArguments( %d ) -> copyFileToBuffer( %s )", pid, path );
		return NULL;
	}

	ptr = strchr ( string, ' ' );
	if ( !ptr )
	{
		free ( string );
		return NULL;
	}

	CMDLine = strdup ( ++ptr );
	free ( string );
	return CMDLine;
}

int lookupProcessLibrary ( pid_t pid, const char **libs )
{
	char path[ PATH_MAX ];
	char *maps;
	int retval;
	int i;
	int reference = INT_MAX / 2;

	if ( !libs )
		return reference;

	sprintf ( path, "/proc/%d/maps", pid );
	retval = copyFileToBuffer ( &maps, path );
	if ( retval < 0 )
	{
		LNCdebug ( LNC_ERROR, 1, "lookupProcessLibrary( %d ) -> copyFileToBuffer( %s )", pid, path );
		return -EXIT_FAILURE;
	}

	for ( i = 0; libs[ i ]; ++i )
	{
		if ( strstr ( maps, libs[ i ] ) )
			reference += 1;
		else
			reference -= 1;
	}

	free ( maps );
	return reference;
}

char *getXauthority ( pid_t PID, char *display )
{
	char *string;
	char *output;
	char file[ PATH_MAX ];
	char *ptr;
	char tmp[ BUFSIZ ];
	char exec[ PATH_MAX ];
	int i, len;
	int exitStatus;
	char buf[ PATH_MAX ];

	if ( PID > 0 )
	{
		sprintf ( file, "/proc/%d/environ", PID );
		len = copyFileToBuffer ( &string, file );
		if ( len < 0 )
		{
			LNCdebug ( LNC_ERROR, 1, "getXauthority( %d ) -> copyFileToBuffer( %s )", PID, file );
			goto xauth;
		}

		for ( i = 0; i < len; i++ )
		{
			if ( string[ i ] == '\0' )
				string[ i ] = '\n';
		}

		ptr = strstr ( string, "XAUTHORITY" );
		if ( ptr )
		{
			sscanf ( ptr, "XAUTHORITY = %s\n", tmp );
			free ( string );
			if ( tmp[ 0 ] == '\0' )
				goto xauth;
			return strdup ( tmp );
		}
		free ( string );
	}

xauth:
	snprintf ( exec, PATH_MAX, "xauth extract %s/.xauth %s", realRoot, display );
	output = execute ( exec, STDOUT_FILENO | STDERR_FILENO, environ, &exitStatus );
	if ( exitStatus != 0 )
	{
		LNCdebug ( LNC_ERROR, 1, "getXauthority( %d ) ->execute( %s ): %s", PID, exec, output );
		if ( output )
			free ( output );
		return NULL;
	}

	free ( output );

	snprintf ( buf, BUFSIZ, "%s/.xauth", realRoot );
	return strdup ( buf );
}

static int openConsole ( const char *filename )
{
	int fd;

	fd = open ( filename, O_RDWR | O_NOCTTY );
	if ( fd < 0 && errno == EACCES )
	{
		fd = open ( filename, O_WRONLY | O_NOCTTY );
		if ( fd < 0 && errno == EACCES )
		{
			fd = open ( filename, O_RDONLY | O_NOCTTY );
			if ( fd < 0 )
				return -EXIT_FAILURE;
		}
	}

	if ( !isatty ( fd ) )
	{
		close ( fd );
		return -EXIT_FAILURE;
	}

	return fd;
}

dev_t getActiveConsole ( void )
{
	int fd;
	int retval;
	char path[ PATH_MAX ];
	struct vt_stat vtbuf;
	struct stat stbuf;

	snprintf ( path, PATH_MAX, "/dev/tty0" );
	fd = openConsole ( path );
	if ( fd < 0 )
	{
		LNCdebug ( LNC_DEBUG, 0, "getActiveConsole(): could not open /dev/tty0" );
		return 0;
	}

	retval = ioctl ( fd, VT_GETSTATE, &vtbuf );
	close ( fd );
	if ( retval < 0 )
	{
		LNCdebug ( LNC_ERROR, 1, "getActiveConsole() -> ioctl( %d, VT_GETSTATE )", fd );
		return 0;
	}

	snprintf ( path, PATH_MAX, "/dev/tty%d", vtbuf.v_active );
	retval = stat ( path, &stbuf );
	if ( retval < 0 )
	{
		LNCdebug ( LNC_ERROR, 1, "getActiveConsole() -> stat( %s )", path );
		return 0;
	}

	if ( !S_ISCHR ( stbuf.st_mode ) )
	{
		LNCdebug ( LNC_ERROR, 0, "getActiveConsole() -> S_ISCHR( %d ): %s is not a tty device", stbuf.st_mode, path );
		return 0;
	}

	return stbuf.st_rdev;
}

char *__getActiveDisplayDevice ( pid_t PID, dev_t activeConsole )
{
	char *buffer;
	char *ptr;
	char *display;
	char path[ PATH_MAX ];
	int i;
	int len;
	int retval;
	dev_t ttyDevice;

	snprintf ( path, PATH_MAX, "/proc/%d/stat", PID );
	len = copyFileToBuffer ( &buffer, path );
	if ( len < 0 )
	{
		LNCdebug ( LNC_ERROR, 1, "__getActiveDisplayDevice( %d, %d ) -> copyFileToBuffer( %s )", PID, activeConsole, path );
		return NULL;
	}

	retval = sscanf ( buffer, "%*d %*s %*c %*d %*d %*d %llu", ( unsigned long long int *) &ttyDevice );
	free ( buffer );
	if ( retval < 0 )
	{
		LNCdebug ( LNC_ERROR, 1, "__getActiveDisplayDevice( %d, %d ) -> sscanf( %s )", PID, activeConsole, buffer );
		return NULL;
	}
	else if ( retval != 1 )
	{
		LNCdebug ( LNC_ERROR, 0, "__getActiveDisplayDevice( %d, %d ) -> sscanf( %s ): Invalid format", PID, activeConsole, buffer );
		return NULL;
	}

	if ( ttyDevice != activeConsole ) {
		LNCdebug( LNC_DEBUG, 0, "__getActiveDisplayDevice( %d, %d ): tty device %d is not equal to activeConsole %d", PID, activeConsole, ttyDevice, activeConsole );
		return NULL;
	}

	snprintf ( path, PATH_MAX, "/proc/%d/cmdline", PID );
	len = copyFileToBuffer ( &buffer, path );
	if ( len < 0 )
	{
		LNCdebug ( LNC_ERROR, 1, "__getActiveDisplayDevice( %d, %d ) -> copyFileToBuffer( %s )", PID, activeConsole, path );
		return NULL;
	}

	for ( i = 0; i < len;  ++i )
	{
		if ( buffer[ i ] == '\0' )
			buffer[ i ] = ' ';
	}

	if ( !( ptr = strstr ( buffer, "X " ) ) && !( ptr = strstr ( buffer, "Xorg " ) ) )
	{
		LNCdebug( LNC_ERROR, 0, "__getActiveDisplayDevice( %d, %d ): Could not find X command in PID %d's cmdline", PID, activeConsole, PID );
		free ( buffer );
		return NULL;
	}

	display = strchr ( ptr, ':' );
	if ( !display )
	{
		free ( buffer );
		return strdup ( ":0" );
	}

	ptr = strchr ( display, ' ' );
	if ( ptr )
		*ptr = '\0';

	display = strdup ( display );
	free ( buffer );

	return display;
}

char *getActiveDisplayDevice ( void )
{
	char *buffer;
	char *display;
	char path[ PATH_MAX ];
	char *tmpPath;
	int len;
	int retval;
	pid_t PID;
	dev_t activeConsole;
	struct dirent *entry;
	DIR *tmpDIR;
	DIR *procDIR;

	display = NULL;

	activeConsole = getActiveConsole();
	if ( activeConsole == 0 )
	{
		LNCdebug ( LNC_ERROR, 1, "getActiveDisplayDevice() -> getActiveConsole()" );
		return NULL;
	}

	tmpPath = getenv( "TMPDIR" );
	if ( !tmpPath )
		tmpPath = "/tmp";

	tmpDIR = opendir ( tmpPath );
	if ( tmpDIR )
	{
		while ( ( entry = readdir ( tmpDIR ) ) )
		{
			len = strlen ( entry->d_name );
			if ( len < 8 || strncmp ( entry->d_name, ".X", 2 ) != 0 || strncmp ( entry->d_name+ ( len -5 ), "-lock", 5 ) != 0 )
				continue;

			snprintf ( path, PATH_MAX, "%s/%s", tmpPath, entry->d_name );
			len = copyFileToBuffer ( &buffer, path );
			if ( len < 0 ) {
				LNCdebug( LNC_ERROR, 1, "getActiveDisplayDevice() -> copyFileToBuffer( %s )", tmpPath );
				continue;
			}

			retval = sscanf ( buffer, "%d", &PID );
			free ( buffer );
			if ( retval < 0 )
			{
				LNCdebug ( LNC_ERROR, 1, "getActiveDisplayDevice() -> sscanf( %s )", buffer );
				continue;
			}
			else if ( retval != 1 )
			{
				LNCdebug ( LNC_ERROR, 0, "getActiveDisplayDevice() -> sscanf( %s ): Invalid format", buffer );
				continue;
			}

			display = __getActiveDisplayDevice ( PID, activeConsole );
			if ( display )
				break;
			else
				LNCdebug( LNC_INFO, 0, "getActiveDisplayDevice() -> __getActiveDisplayDevice( %d, %d ): Could not find active display for X PID %d, start scanning all PIDS", PID, activeConsole, PID );
		}

		closedir ( tmpDIR );
	}
	else
		LNCdebug ( LNC_ERROR, 1, "getActiveDisplayDevice() -> opendir( \"%s\")", tmpPath );

	if ( !display )
	{
		procDIR = opendir ( "/proc" );
		if ( !procDIR )
		{
			LNCdebug ( LNC_ERROR, 1, "getActiveDisplayDevice() -> opendir( \"/proc\")" );
			return NULL;
		}

		while ( ( entry = readdir ( procDIR ) ) )
		{
			if ( entry->d_type != DT_DIR )
				continue;

			retval = sscanf ( entry->d_name, "%d", &PID );
			if ( retval < 0 )
			{
				LNCdebug ( LNC_ERROR, 1, "getActiveDisplayDevice() -> sscanf( %s )", entry->d_name );
				continue;
			}
			else if ( retval != 1 )
				continue;

			if ( PID <= 0 )
				continue;

			display = __getActiveDisplayDevice ( PID, activeConsole );
			if ( display )
				break;

		}

		closedir ( procDIR );
	}

	if ( !display )
		errno = ENODEV;

	return display;
}

char *getKdeinitDisplayDevice ( pid_t SID, struct passwd *user )
{
	char *unixFile;
	char *display;
	char path[ PATH_MAX ];
	DIR *fdDir;
	char inode[ 25 ];
	char *ptr, *end, *alloc;
	char socket1[ PATH_MAX ];
	char socket2[ PATH_MAX ];
	int socket1Length;
	int socket2Length;
	int i;
	int retval;
	struct dirent *fdEntry;
	struct stat buf;

	retval = copyFileToBuffer ( &unixFile, "/proc/net/unix" );
	if ( retval < 0 )
	{
		LNCdebug ( LNC_ERROR, 1, "getKdeinitDisplayDevice( %d ) -> copyFileToBuffer( /proc/net/unix )", SID );
		return NULL;
	}

	for ( i = 0; i < retval; ++i )
	{
		if ( unixFile[ i ] == '\0' )
			unixFile[ i ] = '.';
	}
	unixFile[ i - 1 ] = '\0';

	sprintf ( path, "/proc/%d/fd", SID );
	fdDir = opendir ( path );
	if ( !fdDir )
	{
		LNCdebug ( LNC_ERROR, 1, "getKdeinitDisplay() -> opendir(%s)", path );
		free ( unixFile );
		return NULL;
	}

	while ( ( fdEntry = readdir ( fdDir ) ) )
	{
		if ( fdEntry->d_name[ 0 ] != '.' )
		{
			sprintf ( path, "/proc/%d/fd/%s", SID, fdEntry->d_name );
			retval = stat ( path, &buf );
			if ( retval < 0 )
			{
				LNCdebug ( LNC_ERROR, 1, "getKdeinitDisplay() -> stat(%s)", path );
				closedir ( fdDir );
				free ( unixFile );
				return NULL;
			}

			sprintf ( inode, "%d", ( int ) buf.st_ino );
			sprintf ( socket1, "%s /tmp/ksocket-%s/kdeinit", inode, user->pw_name );
			sprintf ( socket2, "%s %s/tmp/ksocket-%s/kdeinit", inode, user->pw_dir, user->pw_name );
			socket1Length = strlen ( socket1 );
			socket2Length = strlen ( socket2 );
			for ( ptr = unixFile; ( ptr = strstr ( ptr, inode ) ); ++ptr )
			{
				if ( strncmp ( ptr, socket1, socket1Length ) == 0 )
				{
					end = strchr ( ptr, '\n' );
					if ( end )
						* end = '\0';
					display= strchr ( ptr, ':' );
					if ( display )
						display = strdup ( display );
					else if ( ( display = strstr ( ptr, "__" ) ) )
					{
						alloc = malloc ( sizeof ( char ) * ( strlen ( display+2 ) + 2 ) );
						snprintf ( alloc, ( strlen ( display+2 ) + 2 ), ":%s", display+2 );
						display = alloc;
					}

					closedir ( fdDir );
					free ( unixFile );
					return display;
				}
				else if ( strncmp ( ptr, socket2, socket2Length ) == 0 )
				{
					end = strchr ( ptr, '\n' );
					if ( end )
						* end = '\0';
					display= strchr ( ptr, ':' );
					if ( display )
						display = strdup ( display );
					else if ( ( display = strstr ( ptr, "__" ) ) )
					{
						alloc = malloc ( sizeof ( char ) * ( strlen ( display+2 ) + 2 ) );
						snprintf ( alloc, ( strlen ( display+2 ) + 2 ), ":%s", display+2 );
						display = alloc;
					}

					closedir ( fdDir );
					free ( unixFile );
					return display;
				}
			}
		}
	}

	closedir ( fdDir );
	free ( unixFile );
	errno = ENODEV;

	return NULL;
}

char *getDisplayDevice ( pid_t PID, struct passwd *user )
{
	char *string;
	char file[ PATH_MAX ];
	char *ptr;
	char tmp[ BUFSIZ ] = "";
	int i, len;
	pid_t SID;

	if ( PID <= 0 )
		return getActiveDisplayDevice();

	sprintf ( file, "/proc/%d/environ", PID );
	len = copyFileToBuffer ( &string, file );
	if ( len < 0 )
	{
		LNCdebug ( LNC_ERROR, 1, "getDisplayDevice( %d ) -> copyFileToBuffer( %s )", PID, file );
		return NULL;
	}

	for ( i = 0; i < len; i++ )
	{
		if ( string[ i ] == '\0' )
			string[ i ] = '\n';
	}

	ptr = strstr ( string, "DISPLAY" );
	if ( ptr )
	{
		sscanf ( ptr, "DISPLAY = %s\n", tmp );
		free ( string );
		if ( tmp[ 0 ] == '\0' )
			return NULL;
		return strdup ( tmp );
	}
	free ( string );

	LNCdebug ( LNC_INFO, 0, "Try'ing getKdeinitDisplay()" );
	SID = getsid ( PID );
	if ( SID < 0 )
		LNCdebug ( LNC_ERROR, 1, "getDisplayDevice( %d, %s ) -> getsid()", PID, user->pw_name );

	ptr = getKdeinitDisplayDevice ( ( getpgid ( SID ) < 0 ) ? PID : SID, user );
	if ( ptr )
		return ptr;

	return getActiveDisplayDevice();
}

static void clearScreen ( char *ttyDevice )
{
	int status;
	char exec[ ARG_MAX ];
	char *output;

	sprintf ( exec, "clearTTY %s", ttyDevice );
	LNCdebug ( LNC_INFO, 0, "clearScreen( %s ): Clearing screen", ttyDevice );
	output = execute ( exec, STDERR_FILENO, environ, &status );
	if ( status != 0 )
		LNCdebug ( LNC_ERROR, 0, "clearScreen( %s ) -> execute( %s, STDERR_FILENO ): %s", ttyDevice, exec, output );

	if ( output )
		free ( output );

	return ;
}

int Dialog ( const char *URL, struct auth *auth, pid_t PID )
{
	char *display = NULL;
	char *processName = NULL;
	char *Xauthority = NULL;
	char username[ BUFSIZ ] = "";
	char password[ BUFSIZ ] = "";
	char buffer[ BUFSIZ ];
	char ttyBuffer[ PATH_MAX ];
	static char previousURL[ PATH_MAX ] = "";
	int retval;
	int error;
	int ttyFD;
	static int singleShot = 0;
	char *ttyDevice = NULL;
	struct passwd *user;
	GUIPlugin *plugin, *tmp;
	uid_t UID;
	gid_t GID;

	retval = getUIDbyPID ( PID, &UID );
	if ( retval < 0 )
	{
		LNCdebug ( LNC_ERROR, 1, "Dialog( %s ) -> getUIDbyPID( %d )", URL, PID );
		return -EXIT_FAILURE;
	}

	retval = getGIDbyPID ( PID, &GID );
	if ( retval < 0 )
	{
		LNCdebug ( LNC_ERROR, 1, "Dialog( %s ) -> getGIDbyPID( %d )", URL, PID );
		return -EXIT_FAILURE;
	}

	user = getSessionInfoByUID ( UID , buffer );
	if ( !user )
	{
		LNCdebug ( LNC_ERROR, 1, "Dialog( %s ) -> getSessionInfoByUID( %d )", URL, UID );
		return -EXIT_FAILURE;
	}

//	display = getDisplayDevice ( PID, user ); // dolphin can be running on a different display than kio_file, so get the active one
	display = getActiveDisplayDevice();
	if ( display )
	{
		LNCdebug ( LNC_INFO, 0, "DISPLAY = \"%s\"", display );
		Xauthority = getXauthority ( PID, display );
		if ( Xauthority )
			LNCdebug ( LNC_INFO, 0, "XAUTHORITY = \"%s\"", Xauthority );
	}

	ttyDevice = getTTYDeviceByPID ( PID, ttyBuffer, PATH_MAX );
	if ( ttyDevice )
		LNCdebug ( LNC_INFO, 0, "TTY = \"%s\"", ttyDevice );

	tmp = NULL;
	lockList ( guiPluginList, READ_LOCK );
	forEachListEntry ( guiPluginList, plugin )
	{
		LNCdebug ( LNC_DEBUG, 0, "Dialog(): try'ing plugin %s", plugin->type );
		if ( plugin->needsX && !display )
		{
			LNCdebug ( LNC_DEBUG, 0, "Dialog(): This plugin needs a X server, but no display found" );
			continue;
		}
		else if ( !plugin->needsX && !ttyDevice )
		{
			LNCdebug ( LNC_DEBUG, 0, "Dialog(): This plugin needs a console, but no tty found" );
			continue;
		}

		plugin->libraryReference = lookupProcessLibrary ( PID, ( const char ** ) plugin->libraries );
		LNCdebug ( LNC_INFO, 0, "Checked plugin %s, library reference = %d", plugin->type, plugin->libraryReference );
		if ( plugin->libraryReference >= 1 )
		{
			if ( tmp )
			{
				if ( plugin->libraryReference > tmp->libraryReference )
				{
					putListEntry ( guiPluginList, tmp );
					getListEntry ( guiPluginList, plugin );
					tmp = plugin;
				}
			}
			else
			{
				getListEntry ( guiPluginList, plugin );
				tmp = plugin;
			}
		}
	}
	unlockList ( guiPluginList );

	if ( tmp )
		plugin = tmp;
	else
	{
		error = -ENOENT;
		LNCdebug ( LNC_ERROR, 0, "Dialog() -> findListEntry(): Couldn't find prefered plugin\n" );
		plugin = NULL;
		goto out;
	}

	if ( plugin->singleShot )
	{
		if ( strcmp ( URL, previousURL ) != 0 )
		{
			singleShot = 0;
			strlcpy ( previousURL, URL, PATH_MAX );
		}

		if ( singleShot )
		{
			singleShot = 0;
			error = -EACCES;
			goto out;
		}
		else if ( !singleShot )
		{
			singleShot = 1;
		}
	}

	LNCdebug ( LNC_INFO, 0, "Starting loop with plugin \"%s\"", plugin->type );
	/*while ( username[ 0 ] == '\0' )*/
	{
		pid_t childPID;
		int pipefd[ 2 ];
		int retval, size;
		char *ptr, *nptr;
		char buf[ BUFSIZ ] = "";

		error = pipe ( pipefd );
		if ( error )
		{
			LNCdebug ( LNC_ERROR, 1, "Dialog() -> pipe()" );
			goto out;
		}

		childPID = fork();
		if ( childPID < 0 )
		{
			LNCdebug ( LNC_ERROR, 1, "Dialog() -> fork()" );
			error = childPID;
			goto out;
		}
		else if ( childPID == 0 )
		{
			if ( setenv ( "HOME", user->pw_dir, 1 ) < 0 )
			{
				retval = -errno;
				LNCdebug ( LNC_ERROR, 1, "Dialog() -> setenv( HOME = %s )", user->pw_dir );
				goto write;
			}

			if ( setenv ( "TERM", "xterm", 1 ) < 0 )
			{
				retval = -errno;
				LNCdebug ( LNC_ERROR, 1, "Dialog() -> setenv( TERM = %s )", user->pw_dir );
				goto write;
			}

			if ( setenv ( "ESCDELAY", "0", 1 ) < 0 )
			{
				retval = -errno;
				LNCdebug ( LNC_ERROR, 1, "Dialog() -> setenv( ESCDELAY = %s )", user->pw_dir );
				goto write;
			}

			if ( plugin->needsX )
			{
				if ( setenv ( "DISPLAY", display, 1 ) < 0 )
				{
					retval = -errno;
					LNCdebug ( LNC_ERROR, 1, "Dialog() -> setenv( DISPLAY = %s)", display );
					goto write;
				}

				if ( Xauthority )
				{
					if ( setenv ( "XAUTHORITY", Xauthority, 1 ) < 0 )
					{
						retval = -errno;
						LNCdebug ( LNC_ERROR, 1, "Dialog() -> setenv( XAUTHORITY = %s )", Xauthority );
						goto write;
					}
				}
			}

			if ( ttyDevice && !plugin->needsX )
			{
				LNCdebug ( LNC_INFO, 0, "Opening device %s", ttyDevice );

				close ( 0 );
				close ( 1 );
				close ( 2 );

				ttyFD = open ( ttyDevice, O_RDWR | O_NOCTTY );
				if ( ttyFD < 0 )
				{
					retval = -errno;
					LNCdebug ( LNC_ERROR, 1, "Dialog() -> open( %s )", ttyDevice );
					goto write;
				}

				if ( dup2 ( ttyFD, 0 ) < 0 ||
				        dup2 ( ttyFD, 1 ) < 0 ||
				        dup2 ( ttyFD, 2 ) < 0 )
				{
					retval = -errno;
					LNCdebug ( LNC_ERROR, 1, "Dialog() -> dup2()" );
					close ( ttyFD );
					goto write;
				}

				if ( ttyFD > 2 )
					close ( ttyFD );
			}

			processName = getProcessName ( PID );
			if ( !processName )
				processName = strdup ( "Unknown" );

//			if ( strcmp( processName, "kdeinit" ) == 0 )
//				processName = getKdeinitProcessName( PID );
			processName[ 0 ] = toupper ( processName[ 0 ] );

			retval = plugin->dialog ( URL, auth, processName );

		write:
			sprintf ( buf, "retval = %d\n", retval );
			strcat ( buf, "username = " );
			if ( auth->username[ 0 ] != '\0' )
				strcat ( buf, auth->username );
			strcat ( buf, "\n" );

			strcat ( buf, "password = " );
			if ( auth->password )
				strcat ( buf, auth->password );
			strcat ( buf, "\n" );

			close ( pipefd[ 0 ] );
			retval = write ( pipefd[ 1 ], buf, strlen ( buf ) );
			close ( pipefd[ 1 ] );
			close ( 0 );
			close ( 1 );
			close ( 2 );
			lockLNCConfig ( WRITE_LOCK );
			if ( networkConfig.EnableDebugMode )
			{
				unlockLNCConfig();
				execlp ( "sleep", "sleep", "0", NULL );
			}
			else
			{
				unlockLNCConfig();
				_exit ( EXIT_SUCCESS );
			}
		}
		else
		{
			int status;

			error = 0;
			close ( pipefd[ 1 ] );
			size = read ( pipefd[ 0 ], buf, BUFSIZ );
			if ( size < 0 )
			{
				error = -errno;
				LNCdebug ( LNC_ERROR, 1, "Dialog() -> read()" );
			}
			else if ( size == 0 )
			{
				error = -ENOMSG;
				LNCdebug ( LNC_ERROR, 0, "Dialog() -> read(): child wrote nothing" );
			}
			else if ( size == BUFSIZ )
			{
				error = -EOVERFLOW;
				LNCdebug ( LNC_ERROR, 0, "Dialog() -> read(): Output too big" );
			}
			close ( pipefd[ 0 ] );

			while ( wait ( &status ) != childPID )
				;
			if ( WIFSIGNALED ( status ) )
			{
				LNCdebug ( LNC_INFO, 0, "Dialog() ->fork(): received signal %d ( %s )", WTERMSIG ( status ), strsignal ( WTERMSIG ( status ) ) );
				error = -EINTR;
			}

			if ( error != 0 )
				goto out;

			retval = -ESRCH;
			sscanf ( buf, "retval = %d\n", &retval );
			if ( retval < 0 )
			{
				error = retval;
				goto out;
			}

			memset ( username, 0, BUFSIZ );
			memset ( password, 0, BUFSIZ );
			ptr = strchr ( buf, '\n' );
			nptr = strchr ( ++ptr, '\n' );
			*nptr = '\0';
			sscanf ( ptr, "username = %s", username );
			*nptr = '\n';

			ptr = strchr ( ptr, '\n' );
			nptr = strchr ( ++ptr, '\n' );
			*nptr = '\0';
			sscanf ( ptr, "password = %s", password );

			strlcpy ( auth->username, username, NAME_MAX + 1 );
			strlcpy ( auth->password, password, NAME_MAX + 1 );
		}
	}

	error = 0;
out:
	if ( ttyDevice && plugin && !plugin->needsX && plugin->refreshTerminalScreen )
		clearScreen ( ttyDevice );

	if ( display )
		free ( display );
	if ( Xauthority )
		free ( Xauthority );
	if ( plugin )
		putListEntry ( guiPluginList, plugin );

	return error;
}
