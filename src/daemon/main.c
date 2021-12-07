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
#include "nconfig.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>
#include <pwd.h>
#include <mntent.h>
#include <locale.h>
#include <libintl.h>
#include <errno.h>

#include <kdb.h>
#include <kdbbackend.h>

const char *protocolMap[ 12 ] = { "Windows Network", "SMB", "Linux Network", "NFS", "Novell Network", "NCP", "Secure Shell Network", "SSH", "HTTP Nettwork", "HTTP", "FTP Network", "FTP" };


static char *getNetworkMountPath( void ) {
	struct passwd * user;
	struct mntent *mount;
	static char path[ PATH_MAX ];
	FILE *mtab;

	user = getpwuid( geteuid() );
	if ( !user ) {
		fprintf( stderr, "Could not get user information\n" );
		return NULL;
	}

	mtab = setmntent( "/etc/mtab", "r" );
	if ( !mtab ) {
		fprintf( stderr, "Could not open /etc/mtab\n" );
		return NULL;
	}

	while ( ( mount = getmntent( mtab ) ) ) {
		if ( strcmp( mount->mnt_fsname, "netfs" ) != 0 )
			continue;

		if ( strncmp( mount->mnt_dir, user->pw_dir, strlen( user->pw_dir ) ) != 0 )
			continue;

		snprintf( path, PATH_MAX, "%s", mount->mnt_dir );
		endmntent( mtab );

		return path;
	}
	endmntent( mtab );

	return NULL;
}

int main( int argc, char *argv[] ) {
	char buffer[ PATH_MAX ];
	char URL[ PATH_MAX ];
	char name[ PATH_MAX ];
	char *ptr;
	char *string;
	char *protocol;
	char *domain;
	char *host;
	char *share;
	char *networkMountPath;
	int isShortcut;
	int i;
	int value;
	struct key *shortcut;
	struct passwd *user;

	if ( argc != 2 ) {
		fprintf( stderr, " Usage: path_to_network_url [networkclient path]...\n" );
		return EXIT_FAILURE;
	}

	setlocale( LC_ALL, "" );
	textdomain( PACKAGE );
	bindtextdomain( PACKAGE, LOCALE_DIR );

	user = getpwuid( geteuid() );
	if ( !user ) {
		fprintf( stderr, "path_to_network_url( %s ) ->getpwuid( %d ): %s\n", argv[ 1 ], geteuid(), strerror( errno ) );
		return EXIT_FAILURE;
	}

	setenv( "USER", user->pw_name, 1 );

	networkMountPath = getNetworkMountPath();
	if ( !networkMountPath ) {
		fprintf( stderr, "Cound not find a networkclient mount for this user\n" );
		return EXIT_FAILURE;
	}

	snprintf( buffer, PATH_MAX, "%s", argv[ 1 ] );
	string = buffer;
	ptr = strstr( string, networkMountPath );
	if ( !ptr ) {
		fprintf( stderr, "Invalid path\n" );
		return EXIT_FAILURE;
	}

	string = ptr + strlen( networkMountPath );
	ptr = strchr( string, '/' );
	if ( !ptr ) {
		fprintf( stderr, "Invalid path\n" );
		return EXIT_FAILURE;
	}

	protocol = ptr+1;
	string = strchr( ptr+1, '/' );

	isShortcut = 1;
	for ( i = 0; i < 6; ++i ) {
		if ( strncasecmp( protocol, protocolMap[ i + i ], strlen( protocolMap[ i + i ] ) ) == 0 ||
			strncasecmp( protocol, gettext( protocolMap[ i + i ] ), strlen( protocolMap[ i + i ] ) ) == 0 ) {
				protocol = ( char * ) protocolMap[ i + i + 1 ];
				isShortcut = 0;
				break;
			}
	}

	if ( !isShortcut ) {
		snprintf( URL, PATH_MAX, "network://%s%s", protocol, string ? string : "");
		printf( "%s\n", URL );
		return EXIT_SUCCESS;
	}

//parseShortcut:
	snprintf( buffer, PATH_MAX, "%s", argv[ 1 ] );
	string = buffer;
	ptr = strstr( string, networkMountPath );
	if ( !ptr ) {
		fprintf( stderr, "Invalid path\n" );
		return EXIT_FAILURE;
	}

	string = ptr + strlen( networkMountPath );
	while ( *string == '/' )
		++string;

	if ( string[ 0 ] == '\0' ) {
		fprintf( stderr, "Invalid path\n" );
		return EXIT_FAILURE;
	}

	if ( strstr( string, " in" ) ) {
		ptr = string;
		ptr = strstr( ptr, " on " );
		if ( !ptr ) {
			ptr = string;				//C on CENTINA in HEININGA (SMB)"
			share = NULL;
		} else {
			*ptr = '\0';
			share = string;
			string = ptr + 4;
			++ptr;
		}

		ptr = strstr( ptr, " in " );
		if ( !ptr ) {
			fprintf( stderr, "Invalid path\n" );
			return EXIT_FAILURE;
		}
		*ptr = '\0';
		host = string;
		string = ptr;
		++ptr;

		ptr = strstr( ptr, " (" );
		if ( !ptr ) {
			fprintf( stderr, "Invalid path\n" );
			return EXIT_FAILURE;
		}
		*ptr = '\0';
		domain = string + 4;
		string = ptr;
		++ptr;

		ptr = strstr( ptr, ")" );
		if ( !ptr ) {
			fprintf( stderr, "Invalid path\n" );
			return EXIT_FAILURE;
		}
		*ptr = '\0';
		protocol = string + 2;

		if ( share )
			snprintf( URL, PATH_MAX, "network://%s/%s/%s/%s", protocol, domain, host, share );
		else
			snprintf( URL, PATH_MAX, "network://%s/%s/%s", protocol, domain, host );
		printf( "%s\n", URL );
	} else {
		name[ 0 ] = '\0';
		value = 0;


		ptr = string;
		ptr = strstr( ptr, " on " );
		if ( !ptr ) {
			host = string;
			share = NULL;
		} else {
			*ptr = '\0';
			share = string;
			host = ptr + 4;
		}

		ptr = strstr( host, " (" );
		if ( !ptr ) {
			fprintf( stderr, "Invalid path\n" );
			return -EXIT_FAILURE;
		}
		*ptr = '\0';

		protocol = ptr+2;
		ptr = strstr( protocol, ")" );
		if ( !ptr ) {
			fprintf( stderr, "Invalid path\n" );
			return -EXIT_FAILURE;
		}
		*ptr = '\0';

		if ( share )
			snprintf( URL, PATH_MAX, "\\%s\\%s", host, share );
		else
			snprintf( URL, PATH_MAX, "\\%s", host );

		initKeys( "LNC/Shortcuts" );
		rewindKeys( "LNC/Shortcuts", USER_ROOT );
		while ( ( shortcut = forEveryKey2( "LNC/Shortcuts", USER_ROOT ) ) ) {
			if ( ( ptr = strstr( shortcut->name, URL ) ) && strlen( ptr ) == strlen( URL ) && strstr( shortcut->name, protocol )  ) {
				if ( charToLong( shortcut->value ) > value ) {
					value = charToLong( shortcut->value );
					strlcpy( name, shortcut->name, PATH_MAX );
				}
			}

			free( shortcut );
		}
		cleanupKeys( "LNC/Shortcuts" );

		if ( name[ 0 ] != '\0' ) {
			strlcpy( URL, name, PATH_MAX );
			replaceCharacter( URL, '\\', '/' );
			printf( "%s\n", URL );
		}

	}

	return EXIT_SUCCESS;
}
