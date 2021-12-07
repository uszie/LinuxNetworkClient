/***************************************************************************
 *   Copyright (C) 2003 by Usarin Heininga                                 *
 *   usarin@heininga.nl                                                    *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>

#include "networkclient.h"

int authorizationCallback( const char * URL, struct auth * auth ) {
	( void ) URL;

	strcpy( auth->username, "anonymous" );

	return EXIT_SUCCESS;
}
#include <netdb.h>
int main( int argc, char *argv[] ) {
	NETWORK_DIR *dir;
	NETWORK_DIRENT *entry;
	NETWORK_DIRENT tmp;
//	u_int32_t IP;
//	char buffer[ BUFSIZ ];
//	int retval;
//	struct hostent *host;
//	int i;

//	getdomainname( buffer, BUFSIZ );
//	printf("domain = %s\n", buffer );

//	gethostname( buffer, BUFSIZ );
//	printf("host = %s\n", buffer );

//	host = gethostbyname( "usarin.heininga");
//	printf( "%s\n", host->h_name);

//	for ( i = 0;host->h_aliases[ i ]; ++i )
//		printf( "%s\n", host->h_aliases[ i ] );
//	return 0;
	( void ) argc;
	( void ) argv;
	LNCauthorizationCallback = authorizationCallback;
/*	IP = IPtoNumber( "131.220.16.1" );
	printf( "host = %s\n", getHostName( IP, buffer, sizeof( buffer ) ) );
	printf( "domain = %s\n", getDomainName( IP, buffer, sizeof( buffer ) ) );


	IP = IPtoNumber( "131.220.16.1" );
	printf( "host = %s\n", getHostName( IP, buffer, sizeof( buffer ) ) );
	printf( "domain = %s\n", getDomainName( IP, buffer, sizeof( buffer ) ) );
	return 0;*/

	LNCinit( 0, NULL );
	LNCcleanUp();
	return 0;
//	retval =  LNCsymlink( "network://SMB/HEININGA/USARIN/documents", "network://test" );
//	retval = LNCaddManualHost( "network://FTP/ftp.kernel.org", "ftp.kernel.org" );
	dir = LNCopendir( "network://SMB/192.168.3.1", 0 );
	if ( dir )
		LNCclosedir( dir );
	//	printf( "%d\n", retval );
//	sleep( 65 );
	printf( "cont\n");
	LNCcleanUp();
	return 0;
	sleep( 50 );
	dir = LNCopendir( "network://test", 0 );
//	dir = LNCopendir( "network://SSH/(Unknown)/192.168.3.1" );
	if ( !dir ) {
		perror( "LNCopendir" );
		LNCcleanUp();
		return -EXIT_FAILURE;
	}

//	while ( ( entry = LNCreaddir( dir, &tmp ) ) ) {
//		printf( "name = %s\tip = %s\tcomment = %s\n\n", entry->name, entry->IP, entry->comment );
//	}

	LNCclosedir( dir );
	LNCcleanUp();
	/*	int retval;
	DIR *dir;
	struct stat stbuf;

	if ( argc != 3 ) {
		fprintf( stderr, "usage: syscall type path\n" );
		return -EXIT_FAILURE;
	}

	if ( strcmp( argv[ 1 ], "opendir" ) == 0 ) {
		dir = opendir( argv[ 2 ] );
		if ( !dir) {
			fprintf( stderr, "opendir( %s ): %s\n", argv[ 2 ], strerror( errno ) );
			return -EXIT_FAILURE;
		}
		closedir( dir );
	} else if ( strcmp( argv[ 1 ], "stat" ) == 0 ) {
		retval = stat( argv[ 2 ], &stbuf );
		if ( retval < 0) {
			fprintf( stderr, "stat( %s ): %s\n", argv[ 2 ], strerror( errno ) );
			return -EXIT_FAILURE;
		}

	} else if ( strcmp( argv[ 1 ], "readdir" ) == 0 ) {
		dir = opendir( argv[ 2 ] );
		if ( !dir) {
			fprintf( stderr, "readdir( %s ): %s\n", argv[ 2 ], strerror( errno ) );
			return -EXIT_FAILURE;
		}

		if ( !readdir( dir ) ) {
			fprintf( stderr, "readdir( %s ): %s\n", argv[ 2 ], strerror( errno ) );
			closedir( dir );
			return -EXIT_FAILURE;

		}
		closedir( dir );
	}*/

	return EXIT_SUCCESS;
}
