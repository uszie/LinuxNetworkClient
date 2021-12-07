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

int main( int argc, char *argv[] ) {
	char URL[ PATH_MAX ];
	char *ptr;
	char *token;
	char *prevToken;
	char host[ PATH_MAX ];
	char IPBuffer[ 16 ];
	int i;
	u_int32_t IP;

	if ( argc != 2 ) {
		fprintf( stderr, " Usage: lnc_url_to_ip [URL]...\n" );
		return EXIT_FAILURE;
	}

	snprintf( URL, PATH_MAX, argv[ 1 ] );
	if ( strncmp( URL, "network://", 10 ) != 0 ) {
		fprintf( stderr, "Invalid url\n" );
		return EXIT_FAILURE;
	}

	ptr = URL;
	for( i = 1; ( token = strtok( ptr, "/" ) ) && i < 5; ++i ){
		ptr = NULL;

		if ( i == 3 || i == 4 ) {
			if ( isValidIP( token ) ) {
				printf( "%s\n", token );
				return EXIT_SUCCESS;
			}

			if ( i == 3 )
				snprintf( host, PATH_MAX, "%s", token );
			else
				snprintf( host, PATH_MAX, "%s.%s", token, prevToken );

			IP = getIPAddress ( host );
			if ( IP > 0 ) {
				printf( "%s\n", IPtoString( IP, IPBuffer, 16 ) );
				return EXIT_SUCCESS;
			}
		}

		prevToken = token;
	}

	fprintf( stderr, "could not find IP address\n" );

	return EXIT_SUCCESS;
}
