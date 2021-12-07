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
#include <fcntl.h>
#include <string.h>
#include <sys/ioctl.h>

int main( int argc, char *argv[] ) {
//	char *clearScreen = "\f \r";
	char *clearScreen = "\f\f\r\r";
	int fd;
	int retval;

	if ( argc != 2 ) {
		fprintf( stderr, "usage\t\"clearTTY /dev/tty?\"\n" );
		return -EXIT_FAILURE;
	}

	fd = open( argv[ 1 ], O_RDONLY );
	if ( fd < 0 ) {
		perror( "open" );
		return -EXIT_FAILURE;
	}

	while ( *clearScreen ) {
		retval = ioctl( fd, TIOCSTI, clearScreen );
		if ( retval < 0 ) {
			perror( "ioctl" );
			return -EXIT_FAILURE;
		}

//		if ( strstr( argv[ 1 ], "tty" ) )
//			return EXIT_SUCCESS;
		clearScreen++;
	}

	return EXIT_SUCCESS;
}
