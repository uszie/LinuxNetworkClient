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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/types.h>
#include <string.h>
#include <limits.h>
#include <pwd.h>

#include <kdb.h>


int main( int argc, char *argv[] );

int main( int argc, char *argv[] ) {
	struct key *shortcut;
	char key[ PATH_MAX ];
	char value[ BUFSIZ ];
	int retval;
	int64_t usageCount;
	int64_t biggestUsageCount;
	struct passwd *user;

	if ( argc != 2 ) {
		fprintf( stderr, " Usage: createnetworkshortcut [URL]...\n" );
		return EXIT_FAILURE;
	}

	user = getpwuid( geteuid() );
	if ( !user ) {
		fprintf( stderr, "createnetworkshortcut( %s ) ->getpwuid( %d ): %s\n", argv[ 1 ], geteuid(), strerror( errno ) );
		return EXIT_FAILURE;
	}

	setenv( "USER", user->pw_name, 1 );
	biggestUsageCount = 0;

	initKeys( "LNC/Shortcuts" );
	rewindKeys( "LNC/Shortcuts", USER_ROOT );
	while ( ( shortcut = forEveryKey2( "LNC/Shortcuts", USER_ROOT ) ) ) {
		if ( shortcut->type != LNC_CONFIG_KEY )
			continue;

		usageCount = charToLong( shortcut->value );
		if ( usageCount > biggestUsageCount )
			biggestUsageCount = usageCount + 10;
		free( shortcut );
	}


	snprintf( key, PATH_MAX, "%s", argv[ 1 ] );
	replaceCharacter( key, '/', '\\' );
	snprintf( value, BUFSIZ, "%lld", ( long long int ) biggestUsageCount );
	retval = setNumKey( "LNC/Shortcuts", key, biggestUsageCount, USER_ROOT );
	if ( retval < 0 )
		fprintf( stderr, "createnetworkshortcut( %s ) -> setNumKey( LNC/Shortcuts, %s, %ld ): %s\n", argv[ 1 ], key, biggestUsageCount, strerror( errno ) );
	cleanupKeys( "LNC/Shortcuts" );

	return EXIT_SUCCESS;

}
