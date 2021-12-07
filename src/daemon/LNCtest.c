/***************************************************************************
*   Copyright (C) 2003 by Usarin Heininga                                 *
*   usarin@heininga.nl                                                    *
*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License as published by  *
*   the Free Software Foundation; either version 2 of the License, or     *
*   (at your option) any later version.                                   *
***************************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
//#include "plugin.h"

void makeDirs( char *prefix ) {
	int i;
	int retval;
	char path[ PATH_MAX ];

	for ( i = 0; i < 1000; ++i ) {
		snprintf( path, PATH_MAX, "%s/%d", prefix, i );
//		snprintf( path, PATH_MAX, "/home/usarin/test/%d", i );
		retval = mkdir( path, 0777 );
		if ( retval < 0 )
			printf( "mkdir %s = %s\n", path, strerror( errno ) );
	}

	sleep( 5 );

	for ( i = 0; i < 1000; ++i ) {
		snprintf( path, PATH_MAX, "%s/%d", prefix, i );
//		snprintf( path, PATH_MAX, "/home/usarin/test/%d", i );
		retval = rmdir( path );
		if ( retval < 0 )
			printf( "rmdir %s = %s\n", path, strerror( errno ) );
	}
}
#include <stdint.h>
int main( int argc, char *argv[] ) {
//	printf( "%d\t%d\t%d\n", sizeof( void *), sizeof( int ), sizeof( int64_t ) );
	void *test;
	int i;
	( void ) argc;
	( void ) argv;
	test = ( void * ) 4294967295;

	i = ( int64_t ) test;
	printf ("%u\n", i );
//	test = ( void * ) ( int64_t ) i; 
//	test = ( void * ) 32;
//	if ( argc != 2 )
//		return -EXIT_FAILURE;
//	makeDirs( argv[ 1 ] );
	 return EXIT_SUCCESS;
}
