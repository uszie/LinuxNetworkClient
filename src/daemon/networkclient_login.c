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
#include <dirent.h>
#include <errno.h>

int main( int argc, char *argv[] ) {
	DIR * dir;
	char path[ PATH_MAX ];

	if ( argc != 2 ) {
		fprintf( stderr, "Usage: networkclient_login /path/to/dir\n" );
		return EXIT_FAILURE;
	}

	chdir( "/" );
	close( 0 );
	snprintf( path, PATH_MAX, "%s FORCE_AUTHENTICATION", argv[ 1 ] );
	dir = opendir( path );
	if ( !dir ) {
		perror( "opendir" );
		return EXIT_FAILURE;
	}

	closedir( dir );

	return EXIT_SUCCESS;
}
